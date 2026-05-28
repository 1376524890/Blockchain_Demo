#include "consensus/consensus.h"

#include "common/types.h"

#include <algorithm>
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
    // 共识签名体不包含 block 和 signature，仅绑定上下文与 block_hash。
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

ConsensusEngine::ConsensusEngine(NodeConfig config) : config_(std::move(config)) {}

std::string ConsensusEngine::Primary(uint64_t view, uint32_t instance_id) const {
    std::vector<std::string> nodes{config_.node_id};
    for (const auto& p : config_.peers) {
        nodes.push_back(p.node_id);
    }
    return nodes[(view + instance_id) % nodes.size()];
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
    // 投票安全规则：同一 sender 在同一 height/view/instance/type 只能绑定一个 block_hash。
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
    return {
        {"node_id", config_.node_id},
        {"attack_mode", AttackModeToString(attack_mode_)},
        {"running", IsRunning()},
        {"quorum", Quorum()},
        {"evidence_count", evidence_.size()}
    };
}

std::string ConsensusEngine::VoteKey(const ConsensusMessage& msg) const {
    std::ostringstream os;
    os << msg.type << '|' << msg.height << '|' << msg.view << '|' << msg.instance_id << '|' << msg.sender_id;
    return os.str();
}

} // namespace rbft
