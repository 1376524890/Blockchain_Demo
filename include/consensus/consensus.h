#pragma once

#include "block/block.h"
#include "common/config.h"
#include "common/types.h"
#include "datastructure/custom_hash_table.h"
#include "storage/sqlite_storage.h"

#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace rbft {

enum class AttackMode {
    NORMAL,
    BAD_MERKLE_ROOT,
    BAD_STATE_ROOT,
    INVALID_BLOCK_HASH,
    INVALID_NODE_SIGNATURE,
    DROP_PREPARE,
    DROP_COMMIT,
    DELAY_PREPREPARE,
    DOUBLE_PROPOSAL,
    NODE_CRASH_SIMULATED,
    EQUIVOCATION_PREPARE,
    REPLAY_OLD_MESSAGE
};

struct ConsensusMessage {
    std::string msg_id;
    std::string type;              // PRE_PREPARE, PREPARE, COMMIT, VIEW_CHANGE
    std::string chain_id;
    uint64_t height{};
    uint64_t view{};
    uint32_t instance_id{};
    std::string block_hash;
    std::optional<Block> block;    // only in PRE_PREPARE
    std::string sender_id;
    uint64_t timestamp{};
    std::string signature_hex;
};

struct ConsensusEvent {
    uint64_t id{};
    uint64_t timestamp{};
    std::string node_id;
    uint64_t height{};
    uint64_t view{};
    uint32_t instance_id{};
    std::string event_type;
    std::string from;
    std::string to;
    std::string block_hash;
    bool accepted{true};
    std::string reason;
    std::string attack_mode;
};

std::string AttackModeToString(AttackMode mode);
AttackMode AttackModeFromString(const std::string& mode);
std::string SerializeConsensusMessageForSign(const ConsensusMessage& msg);
nlohmann::json ConsensusMessageToJson(const ConsensusMessage& msg);
ConsensusMessage ConsensusMessageFromJson(const nlohmann::json& j);
nlohmann::json ConsensusEventToJson(const ConsensusEvent& event);

// ── PBFT 共识轮次状态 ──
enum class ConsensusPhase { IDLE, PRE_PREPARE, PREPARE, COMMIT, DECIDED };

struct ConsensusRound {
    ConsensusPhase phase{ConsensusPhase::IDLE};
    uint64_t height{};
    uint64_t view{};
    Block proposed_block;
    std::string block_hash;
    int prepare_count{0};
    int commit_count{0};
    std::set<std::string> prepare_senders;
    std::set<std::string> commit_senders;
    std::vector<NodeSignature> commit_signatures;
    uint64_t round_start_time{};
    bool committed{false};
};

class ConsensusEngine {
public:
    explicit ConsensusEngine(NodeConfig config, SQLiteStorage* storage = nullptr);

    // ── 基础功能 ──
    std::string Primary(uint64_t view, uint32_t instance_id) const;
    int Quorum() const;
    void SetAttackMode(AttackMode mode);
    AttackMode GetAttackMode() const;
    bool IsRunning() const;
    void Stop();
    void Start();
    bool RecordVote(const ConsensusMessage& msg, std::string& evidence);
    void AddEvent(ConsensusEvent event);
    std::vector<ConsensusEvent> RecentEvents(size_t limit) const;
    nlohmann::json Status() const;

    // ── PBFT 共识协议 ──
    bool IsPrimary() const;
    uint64_t CurrentView() const;
    ConsensusRound& CurrentRound();
    const ConsensusRound& CurrentRound() const;

    // Primary 创建区块并发起 PRE_PREPARE
    ConsensusMessage CreatePrePrepare(const Block& block);

    // 处理收到的 PRE_PREPARE（backup 节点）
    // 返回 true 表示接受，应广播 PREPARE
    bool OnPrePrepare(const ConsensusMessage& msg, Block& block_out);

    // 处理收到的 PREPARE
    // quorum_reached=true 表示收集到 2f+1 个 PREPARE，应广播 COMMIT
    bool OnPrepare(const ConsensusMessage& msg, bool& quorum_reached);

    // 处理收到的 COMMIT
    // quorum_reached=true 表示收集到 2f+1 个 COMMIT，可以提交区块
    bool OnCommit(const ConsensusMessage& msg, bool& quorum_reached, std::vector<NodeSignature>& sigs);

    // View Change：超时触发
    bool ShouldViewChange() const;
    ConsensusMessage CreateViewChange();
    void OnViewChange(const ConsensusMessage& msg, bool& quorum_reached, uint64_t& new_view);
    void AdvanceView();

    // 提交后重置轮次
    void ResetRound();

    // 获取节点私钥（用于签名共识消息）
    const std::string& GetNodePrivateKey() const { return node_private_key_; }

    // ── 节点隔离（quarantine） ──
    void QuarantineNode(const std::string& node_id, const std::string& reason);
    void UnquarantineNode(const std::string& node_id);
    bool IsQuarantined(const std::string& node_id) const;
    std::vector<std::string> QuarantinedNodes() const;
    void LoadQuarantineFromStorage(SQLiteStorage* storage);

private:
    std::string VoteKey(const ConsensusMessage& msg) const;
    std::string AllNodeIds() const;  // 返回排序后的所有节点 ID
    void BroadcastConsensusMessage(const ConsensusMessage& msg);  // 由 ApiServer 注册的回调

    NodeConfig config_;
    SQLiteStorage* storage_;
    std::string node_private_key_;  // 从文件加载的节点私钥
    AttackMode attack_mode_{AttackMode::NORMAL};
    bool running_{true};

    // 投票记录（equivocation 检测）
    CustomHashTable<std::string, std::string> prepare_votes_{128};
    CustomHashTable<std::string, std::string> commit_votes_{128};
    std::vector<nlohmann::json> evidence_;

    // 事件日志
    std::vector<ConsensusEvent> events_;
    uint64_t next_event_id_{1};

    // 隔离
    std::set<std::string> quarantined_nodes_;

    // ── PBFT 状态 ──
    uint64_t current_view_{0};
    ConsensusRound current_round_;
    std::mutex round_mutex_;

    // VIEW_CHANGE 收集
    std::map<uint64_t, std::set<std::string>> view_change_votes_;  // new_view -> senders
};

} // namespace rbft
