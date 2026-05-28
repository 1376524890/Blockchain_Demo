#pragma once

#include "block/block.h"
#include "common/config.h"
#include "datastructure/custom_hash_table.h"

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
    std::string type;
    std::string chain_id;
    uint64_t height{};
    uint64_t view{};
    uint32_t instance_id{};
    std::string block_hash;
    std::optional<Block> block;
    std::string sender_id;
    uint64_t timestamp{};
    std::string signature_hex;
};

std::string AttackModeToString(AttackMode mode);
AttackMode AttackModeFromString(const std::string& mode);
std::string SerializeConsensusMessageForSign(const ConsensusMessage& msg);
nlohmann::json ConsensusMessageToJson(const ConsensusMessage& msg);
ConsensusMessage ConsensusMessageFromJson(const nlohmann::json& j);

class ConsensusEngine {
public:
    explicit ConsensusEngine(NodeConfig config);

    std::string Primary(uint64_t view, uint32_t instance_id) const;
    int Quorum() const;
    void SetAttackMode(AttackMode mode);
    AttackMode GetAttackMode() const;
    bool IsRunning() const;
    void Stop();
    void Start();
    bool RecordVote(const ConsensusMessage& msg, std::string& evidence);
    nlohmann::json Status() const;

private:
    std::string VoteKey(const ConsensusMessage& msg) const;

    NodeConfig config_;
    AttackMode attack_mode_{AttackMode::NORMAL};
    bool running_{true};
    CustomHashTable<std::string, std::string> prepare_votes_{128};
    CustomHashTable<std::string, std::string> commit_votes_{128};
    std::vector<nlohmann::json> evidence_;
};

} // namespace rbft
