#pragma once

#include "block/block_executor.h"
#include "common/config.h"
#include "consensus/consensus.h"
#include "merkle/merkle_tree.h"
#include "storage/sqlite_storage.h"
#include "tx/mempool.h"
#include "user/user_manager.h"

#include <httplib.h>

namespace rbft {

class ApiServer {
public:
    explicit ApiServer(NodeConfig config);
    void Run();
    void InitDbOnly();

private:
    nlohmann::json Ok(const nlohmann::json& data) const;
    nlohmann::json Err(const std::string& error) const;
    void RegisterRoutes(httplib::Server& server);
    void ReplyJson(httplib::Response& res, int status, const nlohmann::json& body) const;
    void BroadcastBlock(const Block& block);

    NodeConfig config_;
    SQLiteStorage storage_;
    UserManager users_;
    Mempool mempool_;
    ConsensusEngine consensus_;
    BlockExecutor executor_;
};

} // namespace rbft
