#pragma once

#include "block/block_executor.h"
#include "common/config.h"
#include "consensus/consensus.h"
#include "merkle/merkle_tree.h"
#include "storage/sqlite_storage.h"
#include "tx/mempool.h"
#include "user/user_manager.h"

#include <atomic>
#include <httplib.h>
#include <thread>

namespace rbft {

class ApiServer {
public:
    explicit ApiServer(NodeConfig config);
    ~ApiServer();
    void Run();
    void InitDbOnly();

private:
    nlohmann::json Ok(const nlohmann::json& data) const;
    nlohmann::json Err(const std::string& error) const;
    void RegisterRoutes(httplib::Server& server);
    void ReplyJson(httplib::Response& res, int status, const nlohmann::json& body) const;

    // ── P2P 区块广播（用于 catch-up） ──
    void BroadcastBlock(const Block& block);
    void BroadcastBlockToPeer(const Block& block, size_t peer_idx);
    void CatchUpBlocks(const PeerConfig& peer, uint64_t from_height, uint64_t target_height);

    // ── PBFT 共识消息广播 ──
    void BroadcastConsensusMessage(const ConsensusMessage& msg);

    // ── PBFT 共识定时器 ──
    void ConsensusTimerLoop();
    void TryProposeBlock();

    // ── PBFT 区块提交 ──
    void CommitBlockWithSignatures(const Block& block, const std::vector<NodeSignature>& sigs);

    NodeConfig config_;
    SQLiteStorage storage_;
    UserManager users_;
    Mempool mempool_;
    ConsensusEngine consensus_;
    BlockExecutor executor_;

    // PBFT 定时器线程
    std::thread consensus_timer_thread_;
    std::atomic<bool> stop_timer_{false};
};

} // namespace rbft
