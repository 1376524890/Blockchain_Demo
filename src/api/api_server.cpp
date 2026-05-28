#include "api/api_server.h"

#include "common/logger.h"
#include "crypto/crypto_utils.h"

#include <filesystem>

namespace rbft {

ApiServer::ApiServer(NodeConfig config)
    : config_(std::move(config)),
      users_(&storage_),
      consensus_(config_),
      executor_(&storage_) {
    std::filesystem::create_directories(std::filesystem::path(config_.db_path).parent_path());
    storage_.Open(config_.db_path);
    storage_.InitializeSchema();
}

void ApiServer::InitDbOnly() {
    storage_.InitializeSchema();
}

nlohmann::json ApiServer::Ok(const nlohmann::json& data) const {
    return {{"ok", true}, {"data", data}, {"error", ""}};
}

nlohmann::json ApiServer::Err(const std::string& error) const {
    return {{"ok", false}, {"data", nullptr}, {"error", error}};
}

void ApiServer::ReplyJson(httplib::Response& res, int status, const nlohmann::json& body) const {
    res.status = status;
    res.set_content(body.dump(), "application/json; charset=utf-8");
}

void ApiServer::RegisterRoutes(httplib::Server& server) {
    server.Get("/api/node/status", [this](const httplib::Request&, httplib::Response& res) {
        ReplyJson(res, 200, Ok({
            {"node_id", config_.node_id},
            {"rest_port", config_.rest_port},
            {"p2p_port", config_.p2p_port},
            {"latest_height", storage_.GetMetadata("latest_height", "0")},
            {"consensus", consensus_.Status()}
        }));
    });

    server.Get("/api/node/peers", [this](const httplib::Request&, httplib::Response& res) {
        nlohmann::json peers = nlohmann::json::array();
        for (const auto& p : config_.peers) {
            peers.push_back({{"node_id", p.node_id}, {"host", p.host}, {"rest_port", p.rest_port}, {"p2p_port", p.p2p_port}});
        }
        ReplyJson(res, 200, Ok(peers));
    });

    server.Get("/api/node/consensus", [this](const httplib::Request&, httplib::Response& res) {
        ReplyJson(res, 200, Ok(consensus_.Status()));
    });

    server.Post("/api/admin/attack-mode", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = nlohmann::json::parse(req.body);
            consensus_.SetAttackMode(AttackModeFromString(j.value("mode", "normal")));
            ReplyJson(res, 200, Ok({{"mode", AttackModeToString(consensus_.GetAttackMode())}}));
        } catch (const std::exception& e) {
            ReplyJson(res, 400, Err(e.what()));
        }
    });

    server.Post("/api/admin/stop-consensus", [this](const httplib::Request&, httplib::Response& res) {
        consensus_.Stop();
        ReplyJson(res, 200, Ok(consensus_.Status()));
    });

    server.Post("/api/admin/start-consensus", [this](const httplib::Request&, httplib::Response& res) {
        consensus_.Start();
        ReplyJson(res, 200, Ok(consensus_.Status()));
    });

    server.Post("/api/users/register", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = nlohmann::json::parse(req.body);
            auto u = users_.Register(j.at("username").get<std::string>(), j.at("password").get<std::string>());
            // private_key 仅为演示返回，真实系统应由客户端 keystore 保存。
            ReplyJson(res, 200, Ok({{"user_id", u.user_id}, {"username", u.username}, {"address", u.address},
                                   {"public_key", u.public_key_hex}, {"private_key", u.private_key_hex}}));
        } catch (const std::exception& e) {
            ReplyJson(res, 400, Err(e.what()));
        }
    });

    server.Post("/api/users/login", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = nlohmann::json::parse(req.body);
            auto login = users_.Login(j.at("username").get<std::string>(), j.at("password").get<std::string>());
            ReplyJson(res, 200, Ok({{"token", login.token}, {"address", login.user.address},
                                   {"public_key", login.user.public_key_hex}, {"private_key", login.user.private_key_hex}}));
        } catch (const std::exception& e) {
            ReplyJson(res, 401, Err(e.what()));
        }
    });

    server.Get(R"(/api/users/([0-9a-fA-F]+))", [this](const httplib::Request& req, httplib::Response& res) {
        auto u = users_.GetByAddress(req.matches[1]);
        if (!u) {
            ReplyJson(res, 404, Err("user not found"));
            return;
        }
        auto account = storage_.GetAccount(u->address);
        ReplyJson(res, 200, Ok({{"user_id", u->user_id}, {"username", u->username}, {"address", u->address},
                               {"public_key", u->public_key_hex}, {"account", account ? nlohmann::json{{"balance", account->balance}, {"nonce", account->nonce}} : nlohmann::json(nullptr)}}));
    });

    auto add_tx = [this](const httplib::Request& req, httplib::Response& res, const std::string& type) {
        try {
            auto j = nlohmann::json::parse(req.body);
            Transaction tx;
            tx.type = type;
            tx.from = j.at("from").get<std::string>();
            tx.to = j.value("to", "");
            tx.amount = j.value("amount", 0ULL);
            tx.data_hash = j.value("data_hash", "");
            tx.nonce = j.at("nonce").get<uint64_t>();
            tx.timestamp = j.value("timestamp", NowMillis());
            tx.public_key_hex = j.at("public_key").get<std::string>();
            if (j.contains("signature")) {
                tx.signature_hex = j.at("signature").get<std::string>();
            } else if (j.contains("private_key")) {
                // demo-only：便于脚本演示，真实系统必须由客户端本地签名。
                tx.signature_hex = crypto::SignDetachedHex(SerializeTransactionBody(tx), j.at("private_key").get<std::string>());
            } else {
                throw std::runtime_error("missing signature");
            }
            tx.tx_id = ComputeTransactionId(tx);
            std::string error;
            if (!mempool_.AddTransaction(tx, error)) {
                ReplyJson(res, 400, Err(error));
                return;
            }
            storage_.PutTransaction(tx, "PENDING", std::nullopt, std::nullopt);
            if (consensus_.IsRunning() && consensus_.GetAttackMode() == AttackMode::NORMAL) {
                auto picked = mempool_.PickTransactions(100);
                Block block;
                block.transactions = picked;
                block.header.chain_id = config_.chain_id;
                block.header.height = std::stoull(storage_.GetMetadata("latest_height", "0")) + 1;
                auto latest = storage_.GetLatestBlock();
                block.header.previous_block_hash = latest ? latest->header.block_hash : std::string(64, '0');
                block.header.tx_merkle_root = HashToHex(MerkleTree::ComputeRoot(picked));
                block.header.state_root = executor_.ExecuteForStateRoot(picked);
                block.header.timestamp = NowMillis();
                block.header.view = 0;
                block.header.instance_id = 0;
                block.header.proposer_id = config_.node_id;
                block.header.block_hash = ComputeBlockHash(block.header);
                // 演示模式下本地自动提交区块；完整 RBFT 网络投票逻辑由 ConsensusEngine/P2P 接口承载。
                executor_.CommitBlock(block);
                mempool_.RemoveCommitted(picked);
                ReplyJson(res, 200, Ok({{"tx_id", tx.tx_id}, {"status", "COMMITTED"}, {"block_height", block.header.height}}));
                return;
            }
            ReplyJson(res, 200, Ok({{"tx_id", tx.tx_id}, {"status", "PENDING"}}));
        } catch (const std::exception& e) {
            ReplyJson(res, 400, Err(e.what()));
        }
    };

    server.Post("/api/transactions/transfer", [add_tx](const httplib::Request& req, httplib::Response& res) { add_tx(req, res, "TRANSFER"); });
    server.Post("/api/transactions/store", [add_tx](const httplib::Request& req, httplib::Response& res) { add_tx(req, res, "STORE_DATA"); });

    server.Get("/api/transactions/pending", [this](const httplib::Request&, httplib::Response& res) {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& tx : mempool_.Pending()) {
            arr.push_back(TransactionToJson(tx));
        }
        ReplyJson(res, 200, Ok(arr));
    });

    server.Get(R"(/api/transactions/([0-9a-fA-F]+))", [this](const httplib::Request& req, httplib::Response& res) {
        auto tx = storage_.GetTransaction(req.matches[1]);
        if (!tx) ReplyJson(res, 404, Err("transaction not found"));
        else ReplyJson(res, 200, Ok(TransactionToJson(*tx)));
    });

    server.Get("/api/blocks/latest", [this](const httplib::Request&, httplib::Response& res) {
        auto block = storage_.GetLatestBlock();
        if (!block) ReplyJson(res, 404, Err("block not found"));
        else ReplyJson(res, 200, Ok(BlockToJson(*block)));
    });

    server.Get(R"(/api/blocks/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        auto block = storage_.GetBlockByHeight(std::stoull(req.matches[1]));
        if (!block) ReplyJson(res, 404, Err("block not found"));
        else ReplyJson(res, 200, Ok(BlockToJson(*block)));
    });

    server.Get(R"(/api/blocks/hash/([0-9a-fA-F]+))", [this](const httplib::Request& req, httplib::Response& res) {
        auto block = storage_.GetBlockByHash(req.matches[1]);
        if (!block) ReplyJson(res, 404, Err("block not found"));
        else ReplyJson(res, 200, Ok(BlockToJson(*block)));
    });

    server.Get(R"(/api/proofs/tx/([0-9a-fA-F]+))", [this](const httplib::Request& req, httplib::Response& res) {
        auto tx = storage_.GetTransaction(req.matches[1]);
        auto height = storage_.GetTransactionBlockHeight(req.matches[1]);
        if (!tx || !height) {
            ReplyJson(res, 404, Err("committed transaction not found"));
            return;
        }
        auto block = storage_.GetBlockByHeight(*height);
        if (!block) {
            ReplyJson(res, 404, Err("block not found"));
            return;
        }
        size_t index = 0;
        for (; index < block->transactions.size(); ++index) {
            if (block->transactions[index].tx_id == tx->tx_id) break;
        }
        auto proof = MerkleTree::GenerateProof(block->transactions, index);
        nlohmann::json items = nlohmann::json::array();
        for (const auto& item : proof) {
            items.push_back({{"position", item.position == MerkleProofItem::Position::LEFT ? "LEFT" : "RIGHT"},
                             {"hash", HashToHex(item.sibling_hash)}});
        }
        ReplyJson(res, 200, Ok({{"tx", TransactionToJson(*tx)}, {"block_height", *height},
                               {"root", block->header.tx_merkle_root}, {"proof", items}}));
    });

    server.Post("/api/proofs/tx/verify", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = nlohmann::json::parse(req.body);
            auto tx = TransactionFromJson(j.at("tx"));
            std::vector<MerkleProofItem> proof;
            for (const auto& item : j.at("proof")) {
                proof.push_back({item.at("position").get<std::string>() == "LEFT" ? MerkleProofItem::Position::LEFT : MerkleProofItem::Position::RIGHT,
                                 HexToHash(item.at("hash").get<std::string>())});
            }
            ReplyJson(res, 200, Ok({{"valid", MerkleTree::VerifyProof(tx, proof, HexToHash(j.at("root").get<std::string>()))}}));
        } catch (const std::exception& e) {
            ReplyJson(res, 400, Err(e.what()));
        }
    });

    server.Get(R"(/api/state/([0-9a-fA-F]+))", [this](const httplib::Request& req, httplib::Response& res) {
        auto account = storage_.GetAccount(req.matches[1]);
        if (!account) ReplyJson(res, 404, Err("account not found"));
        else ReplyJson(res, 200, Ok({{"address", account->address}, {"balance", account->balance}, {"nonce", account->nonce}}));
    });

    server.Get(R"(/api/state/([0-9a-fA-F]+)/proof)", [this](const httplib::Request& req, httplib::Response& res) {
        auto account = storage_.GetAccount(req.matches[1]);
        if (!account) {
            ReplyJson(res, 404, Err("account not found"));
            return;
        }
        SparseMerkleTree smt;
        auto key = crypto::Sha256String(account->address);
        auto value = EncodeAccountState(*account);
        smt.Update(key, value);
        auto proof = smt.GenerateExistenceProof(key);
        nlohmann::json siblings = nlohmann::json::array();
        for (const auto& h : proof.sibling_hashes) siblings.push_back(HashToHex(h));
        ReplyJson(res, 200, Ok({{"root", HashToHex(smt.GetRoot())}, {"key", HashToHex(key)},
                               {"value", SerializeAccountState(*account)}, {"siblings", siblings}}));
    });

    server.Get(R"(/api/state/([0-9a-fA-F]+)/non-existence-proof)", [this](const httplib::Request& req, httplib::Response& res) {
        SparseMerkleTree smt;
        auto key = crypto::Sha256String(req.matches[1]);
        auto proof = smt.GenerateNonExistenceProof(key);
        nlohmann::json siblings = nlohmann::json::array();
        for (const auto& h : proof.sibling_hashes) siblings.push_back(HashToHex(h));
        ReplyJson(res, 200, Ok({{"root", HashToHex(smt.GetRoot())}, {"key", HashToHex(key)}, {"siblings", siblings}}));
    });

    server.Post("/api/state/proof/verify", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = nlohmann::json::parse(req.body);
            auto key = HexToHash(j.at("key").get<std::string>());
            auto root = HexToHash(j.at("root").get<std::string>());
            std::string value_string = j.at("value").get<std::string>();
            SMTProof proof;
            proof.type = SMTProofType::EXISTENCE;
            for (const auto& h : j.at("siblings")) proof.sibling_hashes.push_back(HexToHash(h.get<std::string>()));
            std::vector<unsigned char> value(value_string.begin(), value_string.end());
            ReplyJson(res, 200, Ok({{"valid", SparseMerkleTree::VerifyExistenceProof(root, key, value, proof)}}));
        } catch (const std::exception& e) {
            ReplyJson(res, 400, Err(e.what()));
        }
    });

    server.Post("/api/state/non-existence-proof/verify", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = nlohmann::json::parse(req.body);
            auto key = HexToHash(j.at("key").get<std::string>());
            auto root = HexToHash(j.at("root").get<std::string>());
            SMTProof proof;
            proof.type = SMTProofType::NON_EXISTENCE;
            for (const auto& h : j.at("siblings")) proof.sibling_hashes.push_back(HexToHash(h.get<std::string>()));
            ReplyJson(res, 200, Ok({{"valid", SparseMerkleTree::VerifyNonExistenceProof(root, key, proof)}}));
        } catch (const std::exception& e) {
            ReplyJson(res, 400, Err(e.what()));
        }
    });

    server.Post("/p2p/consensus/message", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto msg = ConsensusMessageFromJson(nlohmann::json::parse(req.body));
            std::string evidence;
            bool accepted = consensus_.RecordVote(msg, evidence);
            ReplyJson(res, accepted ? 200 : 409, accepted ? Ok({{"accepted", true}}) : Err(evidence));
        } catch (const std::exception& e) {
            ReplyJson(res, 400, Err(e.what()));
        }
    });

    server.Get("/p2p/status", [this](const httplib::Request&, httplib::Response& res) {
        ReplyJson(res, 200, Ok(consensus_.Status()));
    });
}

void ApiServer::Run() {
    httplib::Server server;
    RegisterRoutes(server);
    Logger::Info("listening REST/P2P on port " + std::to_string(config_.rest_port));
    // 演示实现先复用一个 httplib 端口承载 REST 与 P2P 路由；配置仍保留 p2p_port，后续可拆成双 server。
    server.listen("0.0.0.0", config_.rest_port);
}

} // namespace rbft
