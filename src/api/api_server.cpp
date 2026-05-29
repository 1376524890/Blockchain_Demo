#include "api/api_server.h"

#include "common/logger.h"
#include "crypto/crypto_utils.h"

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <thread>

namespace rbft {

namespace {

bool HasRequiredFields(const nlohmann::json& j, const std::vector<std::string>& fields, std::string& missing) {
    for (const auto& field : fields) {
        if (!j.contains(field) || j.at(field).is_null()) {
            missing = field;
            return false;
        }
    }
    return true;
}

bool HashBit(const Hash& key, size_t depth) {
    const size_t byte_index = depth / 8;
    const size_t bit_index = 7 - (depth % 8);
    return ((key[byte_index] >> bit_index) & 1U) == 1U;
}

std::string PathBits(const Hash& key, size_t begin, size_t count) {
    std::string bits;
    bits.reserve(count);
    for (size_t i = begin; i < begin + count && i < 256; ++i) {
        bits.push_back(HashBit(key, i) ? '1' : '0');
    }
    return bits;
}

nlohmann::json ProofToJson(const std::vector<MerkleProofItem>& proof) {
    nlohmann::json items = nlohmann::json::array();
    for (const auto& item : proof) {
        items.push_back({{"position", item.position == MerkleProofItem::Position::LEFT ? "LEFT" : "RIGHT"},
                         {"hash", HashToHex(item.sibling_hash)}});
    }
    return items;
}

nlohmann::json MerkleRecomputeSteps(const Transaction& tx, const std::vector<MerkleProofItem>& proof, Hash& computed_root) {
    nlohmann::json steps = nlohmann::json::array();
    Hash current = MerkleTree::LeafHash(tx);
    for (size_t level = 0; level < proof.size(); ++level) {
        const auto& item = proof[level];
        const auto before = current;
        if (item.position == MerkleProofItem::Position::LEFT) {
            current = MerkleTree::ParentHash(item.sibling_hash, current);
            steps.push_back({{"level", level},
                             {"position", "LEFT"},
                             {"current_before", HashToHex(before)},
                             {"sibling", HashToHex(item.sibling_hash)},
                             {"concat_order", "sibling || current"},
                             {"parent", HashToHex(current)}});
        } else {
            current = MerkleTree::ParentHash(current, item.sibling_hash);
            steps.push_back({{"level", level},
                             {"position", "RIGHT"},
                             {"current_before", HashToHex(before)},
                             {"sibling", HashToHex(item.sibling_hash)},
                             {"concat_order", "current || sibling"},
                             {"parent", HashToHex(current)}});
        }
    }
    computed_root = current;
    return steps;
}

nlohmann::json SMTRecomputeSteps(const Hash& key, Hash current, const SMTProof& proof, Hash& computed_root) {
    nlohmann::json steps = nlohmann::json::array();
    for (size_t depth = 255; depth < 256; --depth) {
        const auto& sibling = proof.sibling_hashes[255 - depth];
        const auto before = current;
        const bool bit = HashBit(key, depth);
        if (bit) {
            current = SparseMerkleTree::ParentHash(sibling, current);
            steps.push_back({{"level", depth}, {"bit", 1}, {"direction", "RIGHT"},
                             {"current_before", HashToHex(before)}, {"sibling", HashToHex(sibling)},
                             {"concat_order", "sibling || current"}, {"parent", HashToHex(current)}});
        } else {
            current = SparseMerkleTree::ParentHash(current, sibling);
            steps.push_back({{"level", depth}, {"bit", 0}, {"direction", "LEFT"},
                             {"current_before", HashToHex(before)}, {"sibling", HashToHex(sibling)},
                             {"concat_order", "current || sibling"}, {"parent", HashToHex(current)}});
        }
    }
    computed_root = current;
    return steps;
}

nlohmann::json TransactionSummaryJson(const Transaction& tx) {
    return {{"tx_id", tx.tx_id}, {"type", tx.type}, {"from", tx.from}, {"to", tx.to},
            {"amount", tx.amount}, {"nonce", tx.nonce}};
}

} // namespace

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
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
    res.set_content(body.dump(), "application/json; charset=utf-8");
}

void ApiServer::RegisterRoutes(httplib::Server& server) {
    server.Options(R"((.*))", [](const httplib::Request&, httplib::Response& res) {
        res.status = 204;
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
    });

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

    server.Get("/api/node/consensus/events", [this](const httplib::Request& req, httplib::Response& res) {
        size_t limit = 100;
        if (req.has_param("limit")) {
            limit = static_cast<size_t>(std::stoull(req.get_param_value("limit")));
        }
        limit = std::min(limit, static_cast<size_t>(1000));
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& event : consensus_.RecentEvents(limit)) {
            arr.push_back(ConsensusEventToJson(event));
        }
        ReplyJson(res, 200, Ok(arr));
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

    server.Post("/api/crypto/generate-keypair", [this](const httplib::Request&, httplib::Response& res) {
        auto kp = crypto::GenerateEd25519KeyPair();
        const std::string address = crypto::Sha256Hex(kp.public_key_hex).substr(0, 40);
        ReplyJson(res, 200, Ok({{"address", address}, {"public_key", kp.public_key_hex}, {"private_key", kp.private_key_hex}}));
    });

    server.Post("/api/crypto/encrypt", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = nlohmann::json::parse(req.body);
            auto plaintext = j.at("plaintext").get<std::string>();
            auto password = j.at("password").get<std::string>();
            auto encrypted = crypto::EncryptSecret(plaintext, password);
            ReplyJson(res, 200, Ok({{"encrypted", encrypted}}));
        } catch (const std::exception& e) {
            ReplyJson(res, 400, Err(e.what()));
        }
    });

    server.Post("/api/crypto/decrypt", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = nlohmann::json::parse(req.body);
            auto encrypted = j.at("encrypted").get<std::string>();
            auto password = j.at("password").get<std::string>();
            auto plaintext = crypto::DecryptSecret(encrypted, password);
            ReplyJson(res, 200, Ok({{"plaintext", plaintext}}));
        } catch (const std::exception& e) {
            ReplyJson(res, 400, Err(e.what()));
        }
    });

    server.Post("/api/users/register", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = nlohmann::json::parse(req.body);
            auto username = j.at("username").get<std::string>();
            auto password = j.at("password").get<std::string>();
            UserRecord u;
            // 如果客户端提供了 address/public_key/private_key，直接使用（多节点同步）
            if (j.contains("address") && j.contains("public_key") && j.contains("private_key")) {
                u = users_.RegisterWithKey(username, password,
                    j.at("address").get<std::string>(),
                    j.at("public_key").get<std::string>(),
                    j.at("private_key").get<std::string>());
            } else {
                u = users_.Register(username, password);
            }
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

    server.Get("/api/users", [this](const httplib::Request& req, httplib::Response& res) {
        size_t page = 1, page_size = 10;
        if (req.has_param("page")) page = std::max(1, std::stoi(req.get_param_value("page")));
        if (req.has_param("page_size")) page_size = std::clamp(std::stoi(req.get_param_value("page_size")), 1, 100);
        size_t offset = (page - 1) * page_size;

        sqlite3_stmt* count_stmt = nullptr;
        sqlite3_prepare_v2(storage_.Raw(), "SELECT COUNT(*) FROM users;", -1, &count_stmt, nullptr);
        sqlite3_step(count_stmt);
        int total = static_cast<int>(sqlite3_column_int64(count_stmt, 0));
        sqlite3_finalize(count_stmt);

        sqlite3_stmt* stmt = nullptr;
        sqlite3_prepare_v2(storage_.Raw(),
            "SELECT u.user_id, u.username, u.address, COALESCE(a.balance, 0), COALESCE(a.nonce, 0) "
            "FROM users u LEFT JOIN accounts a ON u.address = a.address "
            "ORDER BY u.user_id LIMIT ? OFFSET ?;",
            -1, &stmt, nullptr);
        sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(page_size));
        sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(offset));

        nlohmann::json users = nlohmann::json::array();
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            users.push_back({
                {"user_id", sqlite3_column_int64(stmt, 0)},
                {"username", reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1))},
                {"address", reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2))},
                {"balance", static_cast<uint64_t>(sqlite3_column_int64(stmt, 3))},
                {"nonce", static_cast<uint64_t>(sqlite3_column_int64(stmt, 4))}
            });
        }
        sqlite3_finalize(stmt);

        ReplyJson(res, 200, Ok({{"users", users}, {"total", total}, {"page", page}, {"page_size", page_size}}));
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

    server.Post("/api/debug/tx/serialize", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = nlohmann::json::parse(req.body);
            std::string missing;
            const std::vector<std::string> fields{"type", "from", "to", "amount", "data_hash", "nonce", "timestamp", "public_key"};
            if (!HasRequiredFields(j, fields, missing)) {
                ReplyJson(res, 400, Err("missing field: " + missing));
                return;
            }
            Transaction tx;
            tx.type = j.at("type").get<std::string>();
            tx.from = j.at("from").get<std::string>();
            tx.to = j.at("to").get<std::string>();
            tx.amount = j.at("amount").get<uint64_t>();
            tx.data_hash = j.at("data_hash").get<std::string>();
            tx.nonce = j.at("nonce").get<uint64_t>();
            tx.timestamp = j.at("timestamp").get<uint64_t>();
            tx.public_key_hex = j.at("public_key").get<std::string>();
            const auto tx_body = SerializeTransactionBody(tx);
            ReplyJson(res, 200, Ok({{"tx_body", tx_body},
                                   {"body_hash", crypto::Sha256Hex(tx_body)},
                                   {"field_order", fields},
                                   {"note", "tx_id and signature are excluded from tx_body"}}));
        } catch (const std::exception& e) {
            ReplyJson(res, 400, Err(e.what()));
        }
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
            consensus_.AddEvent({0, 0, "", 0, 0, 0, "MEMPOOL_ADD", tx.from, config_.node_id, tx.tx_id, true, "", ""});
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
                consensus_.AddEvent({0, 0, "", block.header.height, block.header.view, block.header.instance_id,
                                     "AUTO_COMMIT_BLOCK", config_.node_id, config_.node_id, block.header.block_hash, true, "", ""});
                mempool_.RemoveCommitted(picked);
                // P2P 同步: 异步广播区块到其他节点
                BroadcastBlock(block);
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

    server.Get(R"(/api/debug/block/(\d+)/trace)", [this](const httplib::Request& req, httplib::Response& res) {
        auto block = storage_.GetBlockByHeight(std::stoull(req.matches[1]));
        if (!block) {
            ReplyJson(res, 404, Err("block not found"));
            return;
        }
        nlohmann::json txs = nlohmann::json::array();
        for (const auto& tx : block->transactions) {
            txs.push_back(TransactionSummaryJson(tx));
        }
        ReplyJson(res, 200, Ok({{"height", block->header.height},
                               {"previous_block_hash", block->header.previous_block_hash},
                               {"transactions", txs},
                               {"tx_merkle_root", block->header.tx_merkle_root},
                               {"state_root", block->header.state_root},
                               {"block_header_serialized", SerializeBlockHeaderForHash(block->header)},
                               {"block_hash", block->header.block_hash},
                               {"view", block->header.view},
                               {"instance_id", block->header.instance_id},
                               {"proposer_id", block->header.proposer_id}}));
    });

    server.Get(R"(/api/debug/merkle/block/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        auto block = storage_.GetBlockByHeight(std::stoull(req.matches[1]));
        if (!block) {
            ReplyJson(res, 404, Err("block not found"));
            return;
        }
        auto levels_hashes = MerkleTree::BuildLevels(block->transactions);
        nlohmann::json levels = nlohmann::json::array();
        for (size_t level = 0; level < levels_hashes.size(); ++level) {
            nlohmann::json nodes = nlohmann::json::array();
            for (size_t i = 0; i < levels_hashes[level].size(); ++i) {
                nlohmann::json node{{"index", i}, {"hash", HashToHex(levels_hashes[level][i])}};
                if (level == 0) {
                    node["source"] = i < block->transactions.size() ? block->transactions[i].tx_id : "";
                    node["node_type"] = "leaf";
                } else {
                    const auto& prev = levels_hashes[level - 1];
                    const size_t left_index = i * 2;
                    const size_t right_index = std::min(left_index + 1, prev.size() - 1);
                    node["left"] = HashToHex(prev[left_index]);
                    node["right"] = HashToHex(prev[right_index]);
                    node["node_type"] = "internal";
                    if (right_index == left_index) {
                        node["duplicated"] = true;
                    }
                }
                nodes.push_back(node);
            }
            levels.push_back(nodes);
        }
        ReplyJson(res, 200, Ok({{"height", block->header.height},
                               {"tx_merkle_root", block->header.tx_merkle_root},
                               {"tx_count", block->transactions.size()},
                               {"levels", levels},
                               {"odd_duplicate_rule", "if a level has odd node count, duplicate the last hash"},
                               {"leaf_rule", "Hash(0x00 || serialized_transaction)"},
                               {"internal_rule", "Hash(0x01 || left_hash || right_hash)"}}));
    });

    server.Get(R"(/api/debug/merkle/tx/([0-9a-fA-F]+))", [this](const httplib::Request& req, httplib::Response& res) {
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
        if (index >= block->transactions.size()) {
            ReplyJson(res, 404, Err("transaction not found in block"));
            return;
        }
        auto proof = MerkleTree::GenerateProof(block->transactions, index);
        Hash computed{};
        auto steps = MerkleRecomputeSteps(*tx, proof, computed);
        ReplyJson(res, 200, Ok({{"tx_id", tx->tx_id},
                               {"block_height", *height},
                               {"tx_index", index},
                               {"tx", TransactionToJson(*tx)},
                               {"leaf_hash", HashToHex(MerkleTree::LeafHash(*tx))},
                               {"expected_root", block->header.tx_merkle_root},
                               {"proof", ProofToJson(proof)},
                               {"recompute_steps", steps},
                               {"computed_root", HashToHex(computed)},
                               {"valid", HashToHex(computed) == block->header.tx_merkle_root}}));
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

    server.Get(R"(/api/debug/smt/([0-9a-fA-F]+))", [this](const httplib::Request& req, httplib::Response& res) {
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
        Hash computed{};
        auto value_hash = crypto::Sha256(value);
        auto steps = SMTRecomputeSteps(key, SparseMerkleTree::LeafHash(key, value_hash), proof, computed);
        const auto root = smt.GetRoot();
        ReplyJson(res, 200, Ok({{"address", account->address},
                               {"key", HashToHex(key)},
                               {"path_bits_prefix", PathBits(key, 0, 8)},
                               {"path_bits_suffix", PathBits(key, 248, 8)},
                               {"value", SerializeAccountState(*account)},
                               {"value_hash", HashToHex(value_hash)},
                               {"root", HashToHex(root)},
                               {"proof_type", "EXISTENCE"},
                               {"siblings", siblings},
                               {"recompute_steps", steps},
                               {"computed_root", HashToHex(computed)},
                               {"valid", computed == root},
                               {"note", "Current SMT proof is demonstration-oriented unless global SMT persistence is enabled."}}));
    });

    server.Get(R"(/api/debug/smt/([0-9a-fA-F]+)/non-existence)", [this](const httplib::Request& req, httplib::Response& res) {
        SparseMerkleTree smt;
        auto key = crypto::Sha256String(req.matches[1]);
        auto proof = smt.GenerateNonExistenceProof(key);
        nlohmann::json siblings = nlohmann::json::array();
        for (const auto& h : proof.sibling_hashes) siblings.push_back(HashToHex(h));
        Hash current = proof.has_collision ? SparseMerkleTree::LeafHash(proof.collision_leaf_key, proof.collision_leaf_value_hash)
                                           : crypto::Sha256String("SMT_EMPTY_LEAF");
        Hash computed{};
        auto steps = SMTRecomputeSteps(key, current, proof, computed);
        const auto root = smt.GetRoot();
        ReplyJson(res, 200, Ok({{"address", req.matches[1].str()},
                               {"key", HashToHex(key)},
                               {"root", HashToHex(root)},
                               {"proof_type", "NON_EXISTENCE"},
                               {"siblings", siblings},
                               {"collision_leaf", nullptr},
                               {"recompute_steps", steps},
                               {"computed_root", HashToHex(computed)},
                               {"valid", SparseMerkleTree::VerifyNonExistenceProof(root, key, proof)},
                               {"note", "Current SMT proof is demonstration-oriented unless global SMT persistence is enabled."}}));
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
            consensus_.AddEvent({0, 0, "", msg.height, msg.view, msg.instance_id, "P2P_MESSAGE_RECEIVED",
                                 msg.sender_id, config_.node_id, msg.block_hash, true, "", ""});
            std::string evidence;
            bool accepted = consensus_.RecordVote(msg, evidence);
            if (!accepted) {
                consensus_.AddEvent({0, 0, "", msg.height, msg.view, msg.instance_id, "RECORD_VOTE_REJECTED",
                                     msg.sender_id, config_.node_id, msg.block_hash, false, evidence, ""});
            }
            ReplyJson(res, accepted ? 200 : 409, accepted ? Ok({{"accepted", true}}) : Err(evidence));
        } catch (const std::exception& e) {
            ReplyJson(res, 400, Err(e.what()));
        }
    });

    server.Get("/p2p/status", [this](const httplib::Request&, httplib::Response& res) {
        ReplyJson(res, 200, Ok(consensus_.Status()));
    });

    // 接收其他节点同步的区块
    server.Post("/p2p/sync/block", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = nlohmann::json::parse(req.body);
            Block block = BlockFromJson(j);

            // 检查高度是否连续
            uint64_t local_height = std::stoull(storage_.GetMetadata("latest_height", "0"));
            if (block.header.height <= local_height) {
                // 已有该区块, 忽略
                ReplyJson(res, 200, Ok({{"status", "IGNORED"}, {"reason", "already have height " + std::to_string(block.header.height)}}));
                return;
            }
            if (block.header.height > local_height + 1) {
                ReplyJson(res, 400, Err("height gap: expected " + std::to_string(local_height + 1) + " got " + std::to_string(block.header.height)));
                return;
            }

            // 验证前一区块哈希
            auto prev = storage_.GetLatestBlock();
            std::string expected_prev = prev ? prev->header.block_hash : std::string(64, '0');
            if (block.header.previous_block_hash != expected_prev) {
                ReplyJson(res, 400, Err("previous_block_hash mismatch"));
                return;
            }

            // 提交区块
            executor_.CommitBlock(block);
            mempool_.RemoveCommitted(block.transactions);
            Logger::Info("synced block #" + std::to_string(block.header.height) + " from " + block.header.proposer_id);
            consensus_.AddEvent({0, 0, "", block.header.height, block.header.view, block.header.instance_id,
                                 "SYNC_BLOCK", block.header.proposer_id, config_.node_id, block.header.block_hash, true, "", ""});
            ReplyJson(res, 200, Ok({{"status", "SYNCED"}, {"height", block.header.height}}));
        } catch (const std::exception& e) {
            ReplyJson(res, 400, Err(e.what()));
        }
    });
}

void ApiServer::Run() {
    httplib::Server server;
    RegisterRoutes(server);
    Logger::Info("listening REST/P2P on port " + std::to_string(config_.rest_port));
    // 演示实现先复用一个 httplib 端口承载 REST 与 P2P 路由；配置仍保留 p2p_port，后续可拆成双 server。
    server.listen("0.0.0.0", config_.rest_port);
}

void ApiServer::BroadcastBlock(const Block& block) {
    // 异步广播区块到所有 peer 节点
    auto block_json = BlockToJson(block).dump();
    for (const auto& peer : config_.peers) {
        std::thread([peer, block_json]() {
            try {
                httplib::Client client(peer.host, peer.rest_port);
                client.set_connection_timeout(3);
                client.set_read_timeout(5);
                auto res = client.Post("/p2p/sync/block", block_json, "application/json");
                if (res && res->status == 200) {
                    Logger::Info("synced block to " + peer.node_id);
                } else {
                    Logger::Warn("sync to " + peer.node_id + " failed: " + (res ? std::to_string(res->status) : "no response"));
                }
            } catch (const std::exception& e) {
                Logger::Warn("sync to " + peer.node_id + " exception: " + e.what());
            }
        }).detach();
    }
}

} // namespace rbft
