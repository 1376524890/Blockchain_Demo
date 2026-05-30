#include "consensus/consensus.h"

#include "common/logger.h"
#include "common/types.h"
#include "crypto/crypto_utils.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace rbft {

std::string AttackModeToString(AttackMode mode) {
    switch (mode) {
        case AttackMode::NORMAL: return "normal";
        case AttackMode::BAD_MERKLE_ROOT: return "bad_merkle_root";
        case AttackMode::BAD_STATE_ROOT: return "bad_state_root";
        case AttackMode::INVALID_BLOCK_HASH: return "invalid_block_hash";
        case AttackMode::INVALID_NODE_SIGNATURE: return "invalid_node_signature";
        case AttackMode::DROP_PREPARE: return "drop_prepare";
        case AttackMode::DROP_COMMIT: return "drop_commit";
        case AttackMode::DELAY_PREPREPARE: return "delay_preprepare";
        case AttackMode::DOUBLE_PROPOSAL: return "double_proposal";
        case AttackMode::NODE_CRASH_SIMULATED: return "node_crash_simulated";
        case AttackMode::EQUIVOCATION_PREPARE: return "equivocation_prepare";
        case AttackMode::REPLAY_OLD_MESSAGE: return "replay_old_message";
    }
    return "normal";
}

AttackMode AttackModeFromString(const std::string& mode) {
    if (mode == "bad_merkle_root") return AttackMode::BAD_MERKLE_ROOT;
    if (mode == "bad_state_root") return AttackMode::BAD_STATE_ROOT;
    if (mode == "invalid_block_hash") return AttackMode::INVALID_BLOCK_HASH;
    if (mode == "invalid_node_signature") return AttackMode::INVALID_NODE_SIGNATURE;
    if (mode == "drop_prepare") return AttackMode::DROP_PREPARE;
    if (mode == "drop_commit") return AttackMode::DROP_COMMIT;
    if (mode == "delay_preprepare") return AttackMode::DELAY_PREPREPARE;
    if (mode == "double_proposal") return AttackMode::DOUBLE_PROPOSAL;
    if (mode == "node_crash_simulated") return AttackMode::NODE_CRASH_SIMULATED;
    if (mode == "equivocation_prepare") return AttackMode::EQUIVOCATION_PREPARE;
    if (mode == "replay_old_message") return AttackMode::REPLAY_OLD_MESSAGE;
    return AttackMode::NORMAL;
}

std::string SerializeConsensusMessageForSign(const ConsensusMessage& msg) {
    std::ostringstream os;
    os << msg.type << '|' << msg.chain_id << '|' << msg.height << '|' << msg.view << '|'
       << msg.instance_id << '|' << msg.block_hash << '|' << msg.sender_id << '|' << msg.timestamp;
    return os.str();
}

nlohmann::json ConsensusMessageToJson(const ConsensusMessage& msg) {
    nlohmann::json j = {
        {"msg_id", msg.msg_id}, {"type", msg.type}, {"chain_id", msg.chain_id},
        {"height", msg.height}, {"view", msg.view}, {"instance_id", msg.instance_id},
        {"block_hash", msg.block_hash}, {"sender_id", msg.sender_id},
        {"timestamp", msg.timestamp}, {"signature", msg.signature_hex}
    };
    if (msg.block) {
        j["block"] = BlockToJson(*msg.block);
    }
    return j;
}

ConsensusMessage ConsensusMessageFromJson(const nlohmann::json& j) {
    ConsensusMessage msg;
    msg.msg_id = j.value("msg_id", "");
    msg.type = j.at("type").get<std::string>();
    msg.chain_id = j.at("chain_id").get<std::string>();
    msg.height = j.at("height").get<uint64_t>();
    msg.view = j.at("view").get<uint64_t>();
    msg.instance_id = j.at("instance_id").get<uint32_t>();
    msg.block_hash = j.value("block_hash", "");
    msg.sender_id = j.at("sender_id").get<std::string>();
    msg.timestamp = j.value("timestamp", NowMillis());
    msg.signature_hex = j.value("signature", "");
    if (j.contains("block") && !j["block"].is_null()) {
        msg.block = BlockFromJson(j["block"]);
    }
    return msg;
}

nlohmann::json ConsensusEventToJson(const ConsensusEvent& event) {
    return {
        {"id", event.id},
        {"timestamp", event.timestamp},
        {"node_id", event.node_id},
        {"height", event.height},
        {"view", event.view},
        {"instance_id", event.instance_id},
        {"event_type", event.event_type},
        {"from", event.from},
        {"to", event.to},
        {"block_hash", event.block_hash},
        {"accepted", event.accepted},
        {"reason", event.reason},
        {"attack_mode", event.attack_mode}
    };
}

// ══════════════════════════════════════════════════════════════════════════════
// 基础功能
// ══════════════════════════════════════════════════════════════════════════════

ConsensusEngine::ConsensusEngine(NodeConfig config, SQLiteStorage* storage)
    : config_(std::move(config)), storage_(storage) {
    // 从文件加载节点私钥
    if (!config_.node_private_key_path.empty()) {
        std::ifstream f(config_.node_private_key_path);
        if (f.is_open()) {
            std::getline(f, node_private_key_);
            // 去除可能的换行符
            while (!node_private_key_.empty() && (node_private_key_.back() == '\n' || node_private_key_.back() == '\r')) {
                node_private_key_.pop_back();
            }
        }
    }
    current_round_.round_start_time = NowMillis();
}

std::string ConsensusEngine::Primary(uint64_t view, uint32_t instance_id) const {
    // 使用排序后的节点列表，确保所有节点计算出相同的 Primary
    // 跳过被隔离的节点
    std::vector<std::string> nodes{config_.node_id};
    for (const auto& p : config_.peers) {
        nodes.push_back(p.node_id);
    }
    std::sort(nodes.begin(), nodes.end());
    // 过滤掉被隔离的节点
    std::vector<std::string> active_nodes;
    for (const auto& nid : nodes) {
        if (quarantined_nodes_.count(nid) == 0) {
            active_nodes.push_back(nid);
        }
    }
    if (active_nodes.empty()) {
        // 所有节点都被隔离，回退到原始列表
        return nodes[(view + instance_id) % nodes.size()];
    }
    return active_nodes[(view + instance_id) % active_nodes.size()];
}

int ConsensusEngine::Quorum() const {
    return 2 * config_.f + 1;
}

void ConsensusEngine::SetAttackMode(AttackMode mode) {
    attack_mode_ = mode;
    AddEvent({0, 0, "", 0, 0, 0, "ATTACK_MODE_SET", config_.node_id, config_.node_id, "", true, "", ""});
}
AttackMode ConsensusEngine::GetAttackMode() const { return attack_mode_; }
bool ConsensusEngine::IsRunning() const { return running_ && attack_mode_ != AttackMode::NODE_CRASH_SIMULATED; }
void ConsensusEngine::Stop() {
    running_ = false;
    AddEvent({0, 0, "", 0, 0, 0, "CONSENSUS_STOP", config_.node_id, config_.node_id, "", true, "", ""});
}
void ConsensusEngine::Start() {
    running_ = true;
    AddEvent({0, 0, "", 0, 0, 0, "CONSENSUS_START", config_.node_id, config_.node_id, "", true, "", ""});
}

bool ConsensusEngine::RecordVote(const ConsensusMessage& msg, std::string& evidence) {
    auto key = VoteKey(msg);
    auto& table = (msg.type == "COMMIT") ? commit_votes_ : prepare_votes_;
    auto old = table.Get(key);
    if (old && *old != msg.block_hash) {
        nlohmann::json ev = {
            {"evidence_type", "conflicting_vote"},
            {"sender_id", msg.sender_id},
            {"height", msg.height},
            {"view", msg.view},
            {"instance_id", msg.instance_id},
            {"old_block_hash", *old},
            {"new_block_hash", msg.block_hash}
        };
        evidence_.push_back(ev);
        evidence = ev.dump();
        AddEvent({0, 0, "", msg.height, msg.view, msg.instance_id, "CONFLICTING_VOTE",
                  msg.sender_id, config_.node_id, msg.block_hash, false, evidence, ""});
        return false;
    }
    table.Put(key, msg.block_hash);
    AddEvent({0, 0, "", msg.height, msg.view, msg.instance_id, msg.type,
              msg.sender_id, config_.node_id, msg.block_hash, true, "", ""});
    return true;
}

void ConsensusEngine::AddEvent(ConsensusEvent event) {
    event.id = next_event_id_++;
    event.timestamp = event.timestamp == 0 ? NowMillis() : event.timestamp;
    event.node_id = event.node_id.empty() ? config_.node_id : event.node_id;
    event.attack_mode = event.attack_mode.empty() ? AttackModeToString(attack_mode_) : event.attack_mode;
    events_.push_back(std::move(event));
    if (events_.size() > 1000) {
        events_.erase(events_.begin(), events_.begin() + static_cast<std::ptrdiff_t>(events_.size() - 1000));
    }
}

std::vector<ConsensusEvent> ConsensusEngine::RecentEvents(size_t limit) const {
    const size_t count = std::min(limit, events_.size());
    return std::vector<ConsensusEvent>(events_.end() - static_cast<std::ptrdiff_t>(count), events_.end());
}

nlohmann::json ConsensusEngine::Status() const {
    nlohmann::json qnodes = nlohmann::json::array();
    for (const auto& nid : quarantined_nodes_) {
        qnodes.push_back(nid);
    }
    return {
        {"node_id", config_.node_id},
        {"attack_mode", AttackModeToString(attack_mode_)},
        {"running", IsRunning()},
        {"quorum", Quorum()},
        {"evidence_count", evidence_.size()},
        {"quarantined_nodes", qnodes},
        {"current_view", current_view_},
        {"is_primary", IsPrimary()},
        {"consensus_phase", static_cast<int>(current_round_.phase)},
        {"round_height", current_round_.height},
        {"prepare_count", current_round_.prepare_count},
        {"commit_count", current_round_.commit_count}
    };
}

// ══════════════════════════════════════════════════════════════════════════════
// PBFT 共识协议
// ══════════════════════════════════════════════════════════════════════════════

bool ConsensusEngine::IsPrimary() const {
    return Primary(current_view_, 0) == config_.node_id;
}

uint64_t ConsensusEngine::CurrentView() const {
    return current_view_;
}

ConsensusRound& ConsensusEngine::CurrentRound() {
    return current_round_;
}

const ConsensusRound& ConsensusEngine::CurrentRound() const {
    return current_round_;
}

ConsensusMessage ConsensusEngine::CreatePrePrepare(const Block& block) {
    std::lock_guard<std::mutex> lock(round_mutex_);
    current_round_.phase = ConsensusPhase::PRE_PREPARE;
    current_round_.height = block.header.height;
    current_round_.view = current_view_;
    current_round_.proposed_block = block;
    current_round_.block_hash = block.header.block_hash;
    current_round_.prepare_count = 0;
    current_round_.commit_count = 0;
    current_round_.prepare_senders.clear();
    current_round_.commit_senders.clear();
    current_round_.commit_signatures.clear();
    current_round_.round_start_time = NowMillis();
    current_round_.committed = false;

    ConsensusMessage msg;
    msg.msg_id = config_.node_id + ":" + std::to_string(block.header.height) + ":" + std::to_string(current_view_);
    msg.type = "PRE_PREPARE";
    msg.chain_id = config_.chain_id;
    msg.height = block.header.height;
    msg.view = current_view_;
    msg.instance_id = 0;
    msg.block_hash = block.header.block_hash;
    msg.block = block;
    msg.sender_id = config_.node_id;
    msg.timestamp = NowMillis();
    // 签名
    msg.signature_hex = crypto::SignDetachedHex(SerializeConsensusMessageForSign(msg), node_private_key_);

    AddEvent({0, 0, "", block.header.height, current_view_, 0, "PRE_PREPARE_SENT",
              config_.node_id, "", block.header.block_hash, true, "", ""});

    return msg;
}

bool ConsensusEngine::OnPrePrepare(const ConsensusMessage& msg, Block& block_out) {
    std::lock_guard<std::mutex> lock(round_mutex_);

    // 只接受当前 primary 发来的 PRE_PREPARE
    std::string expected_primary = Primary(msg.view, msg.instance_id);
    if (msg.sender_id != expected_primary) {
        Logger::Warn("PRE_PREPARE rejected: sender " + msg.sender_id + " is not primary (expected " + expected_primary + ")");
        AddEvent({0, 0, "", msg.height, msg.view, msg.instance_id, "PRE_PREPARE_REJECTED",
                  msg.sender_id, config_.node_id, msg.block_hash, false, "not primary", ""});
        return false;
    }

    // 检查高度
    if (msg.height <= std::stoull(storage_->GetMetadata("latest_height", "0"))) {
        return false;  // 已过时
    }

    // 设置轮次状态
    current_round_.phase = ConsensusPhase::PRE_PREPARE;
    current_round_.height = msg.height;
    current_round_.view = msg.view;
    current_round_.block_hash = msg.block_hash;
    current_round_.round_start_time = NowMillis();

    if (msg.block) {
        current_round_.proposed_block = *msg.block;
        block_out = *msg.block;
    }

    AddEvent({0, 0, "", msg.height, msg.view, msg.instance_id, "PRE_PREPARE_ACCEPTED",
              msg.sender_id, config_.node_id, msg.block_hash, true, "", ""});

    return true;
}

bool ConsensusEngine::OnPrepare(const ConsensusMessage& msg, bool& quorum_reached) {
    std::lock_guard<std::mutex> lock(round_mutex_);
    quorum_reached = false;

    // equivocation 检测
    std::string evidence;
    if (!RecordVote(msg, evidence)) {
        QuarantineNode(msg.sender_id, "equivocation in PREPARE at height " + std::to_string(msg.height));
        return false;
    }

    // DROP_PREPARE 攻击：收到 PREPARE 但不计入
    if (attack_mode_ == AttackMode::DROP_PREPARE) {
        Logger::Warn("attack: dropping PREPARE from " + msg.sender_id);
        return false;
    }

    // 只接受与当前轮次匹配的 PREPARE
    if (msg.height != current_round_.height || msg.view != current_round_.view || msg.block_hash != current_round_.block_hash) {
        return false;
    }

    // 防止重复计数
    if (current_round_.prepare_senders.count(msg.sender_id)) {
        return false;
    }

    current_round_.prepare_senders.insert(msg.sender_id);
    current_round_.prepare_count = static_cast<int>(current_round_.prepare_senders.size());

    AddEvent({0, 0, "", msg.height, msg.view, msg.instance_id, "PREPARE_RECEIVED",
              msg.sender_id, config_.node_id, msg.block_hash, true,
              "count=" + std::to_string(current_round_.prepare_count), ""});

    // 检查是否达到 quorum
    if (current_round_.prepare_count >= Quorum()) {
        current_round_.phase = ConsensusPhase::COMMIT;
        quorum_reached = true;
        AddEvent({0, 0, "", msg.height, msg.view, msg.instance_id, "PREPARE_QUORUM",
                  config_.node_id, config_.node_id, msg.block_hash, true, "", ""});
    }

    return true;
}

bool ConsensusEngine::OnCommit(const ConsensusMessage& msg, bool& quorum_reached, std::vector<NodeSignature>& sigs) {
    std::lock_guard<std::mutex> lock(round_mutex_);
    quorum_reached = false;

    // equivocation 检测
    std::string evidence;
    if (!RecordVote(msg, evidence)) {
        QuarantineNode(msg.sender_id, "equivocation in COMMIT at height " + std::to_string(msg.height));
        return false;
    }

    // DROP_COMMIT 攻击
    if (attack_mode_ == AttackMode::DROP_COMMIT) {
        Logger::Warn("attack: dropping COMMIT from " + msg.sender_id);
        return false;
    }

    if (msg.height != current_round_.height || msg.view != current_round_.view || msg.block_hash != current_round_.block_hash) {
        return false;
    }

    if (current_round_.commit_senders.count(msg.sender_id)) {
        return false;
    }

    current_round_.commit_senders.insert(msg.sender_id);
    current_round_.commit_count = static_cast<int>(current_round_.commit_senders.size());

    // 收集签名
    NodeSignature sig;
    sig.node_id = msg.sender_id;
    sig.signature_hex = msg.signature_hex;
    current_round_.commit_signatures.push_back(sig);

    AddEvent({0, 0, "", msg.height, msg.view, msg.instance_id, "COMMIT_RECEIVED",
              msg.sender_id, config_.node_id, msg.block_hash, true,
              "count=" + std::to_string(current_round_.commit_count), ""});

    if (current_round_.commit_count >= Quorum()) {
        current_round_.phase = ConsensusPhase::DECIDED;
        current_round_.committed = true;
        quorum_reached = true;
        sigs = current_round_.commit_signatures;
        AddEvent({0, 0, "", msg.height, msg.view, msg.instance_id, "COMMIT_QUORUM",
                  config_.node_id, config_.node_id, msg.block_hash, true, "", ""});
    }

    return true;
}

// ── View Change ──

bool ConsensusEngine::ShouldViewChange() const {
    if (current_round_.phase == ConsensusPhase::IDLE || current_round_.committed) {
        return false;
    }
    // 防止 View Change 风暴：超过最大 view 次数就停止
    if (current_view_ >= 3) {
        return false;  // 最多尝试 3 次 view change
    }
    uint64_t elapsed = NowMillis() - current_round_.round_start_time;
    return elapsed > static_cast<uint64_t>(config_.consensus_timeout_ms);
}

ConsensusMessage ConsensusEngine::CreateViewChange() {
    ConsensusMessage msg;
    msg.msg_id = config_.node_id + ":vc:" + std::to_string(current_view_ + 1);
    msg.type = "VIEW_CHANGE";
    msg.chain_id = config_.chain_id;
    msg.height = current_round_.height;
    msg.view = current_view_ + 1;  // 目标新 view
    msg.instance_id = 0;
    msg.block_hash = current_round_.block_hash;
    msg.sender_id = config_.node_id;
    msg.timestamp = NowMillis();
    msg.signature_hex = crypto::SignDetachedHex(SerializeConsensusMessageForSign(msg), node_private_key_);

    AddEvent({0, 0, "", current_round_.height, current_view_ + 1, 0, "VIEW_CHANGE_SENT",
              config_.node_id, "", "", true, "", ""});

    return msg;
}

void ConsensusEngine::OnViewChange(const ConsensusMessage& msg, bool& quorum_reached, uint64_t& new_view) {
    std::lock_guard<std::mutex> lock(round_mutex_);
    quorum_reached = false;

    uint64_t target_view = msg.view;
    view_change_votes_[target_view].insert(msg.sender_id);

    AddEvent({0, 0, "", msg.height, target_view, 0, "VIEW_CHANGE_RECEIVED",
              msg.sender_id, config_.node_id, "", true,
              "count=" + std::to_string(view_change_votes_[target_view].size()), ""});

    if (static_cast<int>(view_change_votes_[target_view].size()) >= Quorum()) {
        quorum_reached = true;
        new_view = target_view;
        AddEvent({0, 0, "", msg.height, target_view, 0, "VIEW_CHANGE_QUORUM",
                  config_.node_id, config_.node_id, "", true, "", ""});
    }
}

void ConsensusEngine::AdvanceView() {
    std::lock_guard<std::mutex> lock(round_mutex_);
    current_view_++;
    current_round_ = ConsensusRound{};  // 重置轮次
    current_round_.round_start_time = NowMillis();
    Logger::Info("view advanced to " + std::to_string(current_view_) + ", new primary: " + Primary(current_view_, 0));
    AddEvent({0, 0, "", 0, current_view_, 0, "VIEW_ADVANCED",
              config_.node_id, config_.node_id, "", true,
              "new_primary=" + Primary(current_view_, 0), ""});
}

void ConsensusEngine::ResetRound() {
    std::lock_guard<std::mutex> lock(round_mutex_);
    current_round_ = ConsensusRound{};
    current_round_.round_start_time = NowMillis();
}

std::string ConsensusEngine::VoteKey(const ConsensusMessage& msg) const {
    std::ostringstream os;
    os << msg.type << '|' << msg.height << '|' << msg.view << '|' << msg.instance_id << '|' << msg.sender_id;
    return os.str();
}

std::string ConsensusEngine::AllNodeIds() const {
    std::vector<std::string> nodes{config_.node_id};
    for (const auto& p : config_.peers) {
        nodes.push_back(p.node_id);
    }
    std::sort(nodes.begin(), nodes.end());
    std::ostringstream os;
    for (size_t i = 0; i < nodes.size(); ++i) {
        if (i > 0) os << ',';
        os << nodes[i];
    }
    return os.str();
}

// ── 节点隔离（quarantine） ──

void ConsensusEngine::QuarantineNode(const std::string& node_id, const std::string& reason) {
    if (quarantined_nodes_.count(node_id)) return;
    quarantined_nodes_.insert(node_id);
    if (storage_) {
        storage_->PutMetadata("quarantine:" + node_id, reason);
    }
    AddEvent({0, 0, "", 0, 0, 0, "NODE_QUARANTINED", node_id, config_.node_id, "", false, reason, ""});
}

void ConsensusEngine::UnquarantineNode(const std::string& node_id) {
    quarantined_nodes_.erase(node_id);
    if (storage_) {
        storage_->PutMetadata("quarantine:" + node_id, "");
    }
    AddEvent({0, 0, "", 0, 0, 0, "NODE_UNQUARANTINED", node_id, config_.node_id, "", true, "", ""});
}

bool ConsensusEngine::IsQuarantined(const std::string& node_id) const {
    return quarantined_nodes_.count(node_id) > 0;
}

std::vector<std::string> ConsensusEngine::QuarantinedNodes() const {
    return {quarantined_nodes_.begin(), quarantined_nodes_.end()};
}

void ConsensusEngine::LoadQuarantineFromStorage(SQLiteStorage* storage) {
    if (!storage) return;
    quarantined_nodes_.clear();
    auto check = [&](const std::string& nid) {
        auto reason = storage->GetMetadata("quarantine:" + nid, "");
        if (!reason.empty()) {
            quarantined_nodes_.insert(nid);
        }
    };
    check(config_.node_id);
    for (const auto& p : config_.peers) {
        check(p.node_id);
    }
}

} // namespace rbft
