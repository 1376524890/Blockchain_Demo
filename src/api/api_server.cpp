#include "api/api_server.h"

#include "common/logger.h"
#include "crypto/crypto_utils.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <future>
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
      consensus_(config_, &storage_),
      executor_(&storage_) {
    std::filesystem::create_directories(std::filesystem::path(config_.db_path).parent_path());
    storage_.Open(config_.db_path);
    storage_.InitializeSchema();
    consensus_.LoadQuarantineFromStorage(&storage_);
    // 启动 PBFT 共识定时器
    consensus_timer_thread_ = std::thread([this]() { ConsensusTimerLoop(); });
}

ApiServer::~ApiServer() {
    stop_timer_ = true;
    if (consensus_timer_thread_.joinable()) {
        consensus_timer_thread_.join();
    }
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

    // ── 隔离管理端点 ──
    server.Post("/api/admin/quarantine", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = nlohmann::json::parse(req.body);
            auto node_id = j.at("node_id").get<std::string>();
            auto reason = j.value("reason", "manual quarantine");
            consensus_.QuarantineNode(node_id, reason);
            ReplyJson(res, 200, Ok({{"quarantined", node_id}, {"reason", reason}}));
        } catch (const std::exception& e) {
            ReplyJson(res, 400, Err(e.what()));
        }
    });

    server.Post("/api/admin/unquarantine", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = nlohmann::json::parse(req.body);
            auto node_id = j.at("node_id").get<std::string>();
            consensus_.UnquarantineNode(node_id);
            ReplyJson(res, 200, Ok({{"unquarantined", node_id}}));
        } catch (const std::exception& e) {
            ReplyJson(res, 400, Err(e.what()));
        }
    });

    server.Get("/api/admin/quarantine", [this](const httplib::Request&, httplib::Response& res) {
        auto nodes = consensus_.QuarantinedNodes();
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& nid : nodes) {
            auto reason = storage_.GetMetadata("quarantine:" + nid, "");
            arr.push_back({{"node_id", nid}, {"reason", reason}});
        }
        ReplyJson(res, 200, Ok({{"quarantined_nodes", arr}}));
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

            // PBFT 模式：交易加入本地 mempool
            // 如果当前节点不是 Primary，异步转发给 Primary
            // Primary 的定时器会从 mempool 出块
            if (consensus_.IsRunning() && !consensus_.IsPrimary()) {
                std::string primary_id = consensus_.Primary(consensus_.CurrentView(), 0);
                for (const auto& peer : config_.peers) {
                    if (peer.node_id == primary_id) {
                        auto tx_json = req.body;
                        auto p = peer;
                        auto t = type;
                        std::thread([p, tx_json, t]() {
                            try {
                                httplib::Client client(p.host, p.rest_port);
                                client.set_connection_timeout(1);
                                client.set_read_timeout(2);
                                std::string path = (t == "TRANSFER") ? "/api/transactions/transfer" : "/api/transactions/store";
                                client.Post(path, tx_json, "application/json");
                            } catch (...) {}
                        }).detach();
                        break;
                    }
                }
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
            consensus_.AddEvent({0, 0, "", msg.height, msg.view, msg.instance_id, "P2P_" + msg.type,
                                 msg.sender_id, config_.node_id, msg.block_hash, true, "", ""});

            if (msg.type == "PRE_PREPARE") {
                // ── 收到 PRE_PREPARE（来自 Primary） ──
                Block block;
                if (!consensus_.OnPrePrepare(msg, block)) {
                    ReplyJson(res, 400, Err("PRE_PREPARE rejected"));
                    return;
                }
                // 验证区块完整性
                std::string real_merkle = HashToHex(MerkleTree::ComputeRoot(block.transactions));
                if (real_merkle != block.header.tx_merkle_root) {
                    Logger::Warn("PRE_PREPARE rejected: merkle mismatch");
                    consensus_.QuarantineNode(msg.sender_id, "PRE_PREPARE merkle mismatch");
                    consensus_.ResetRound();  // 重置轮次，防止新 Primary 无法出块
                    ReplyJson(res, 403, Err("merkle mismatch"));
                    return;
                }
                try {
                    std::string real_state = executor_.ExecuteForStateRoot(block.transactions);
                    if (real_state != block.header.state_root) {
                        Logger::Warn("PRE_PREPARE rejected: state_root mismatch");
                        consensus_.QuarantineNode(msg.sender_id, "PRE_PREPARE state_root mismatch");
                        consensus_.ResetRound();
                        ReplyJson(res, 403, Err("state_root mismatch"));
                        return;
                    }
                } catch (const std::exception& e) {
                    Logger::Warn("PRE_PREPARE rejected: state_root execution failed: " + std::string(e.what()));
                    consensus_.ResetRound();
                    ReplyJson(res, 403, Err("state_root execution failed: " + std::string(e.what())));
                    return;
                }
                BlockHeader hdr = block.header;
                std::string saved_hash = hdr.block_hash;
                hdr.block_hash = "";
                if (ComputeBlockHash(hdr) != saved_hash) {
                    Logger::Warn("PRE_PREPARE rejected: block_hash mismatch");
                    consensus_.QuarantineNode(msg.sender_id, "PRE_PREPARE block_hash mismatch");
                    consensus_.ResetRound();
                    ReplyJson(res, 403, Err("block_hash mismatch"));
                    return;
                }
                // 验证通过，返回 200（Primary 会统计 200 响应作为 PREPARE 投票）
                ReplyJson(res, 200, Ok({{"accepted", true}, {"phase", "PREPARE"}}));
                return;

            } else if (msg.type == "PREPARE") {
                // ── 收到 PREPARE ──
                bool quorum = false;
                if (!consensus_.OnPrepare(msg, quorum)) {
                    ReplyJson(res, 400, Err("PREPARE rejected"));
                    return;
                }
                if (quorum) {
                    // 达到 quorum，广播 COMMIT
                ConsensusMessage commit;
                    commit.msg_id = config_.node_id + ":" + std::to_string(msg.height) + ":" + std::to_string(msg.view) + ":commit";
                    commit.type = "COMMIT";
                    commit.chain_id = config_.chain_id;
                    commit.height = msg.height;
                    commit.view = msg.view;
                    commit.instance_id = msg.instance_id;
                    commit.block_hash = msg.block_hash;
                    commit.sender_id = config_.node_id;
                    commit.timestamp = NowMillis();
                    commit.signature_hex = crypto::SignDetachedHex(SerializeConsensusMessageForSign(commit), consensus_.GetNodePrivateKey());
                    BroadcastConsensusMessage(commit);
                }
                ReplyJson(res, 200, Ok({{"accepted", true}, {"phase", "PREPARE"}, {"quorum", quorum}}));

            } else if (msg.type == "COMMIT") {
                // ── 收到 COMMIT（来自 Primary） ──
                // 备份节点：验证区块并提交
                const auto& round = consensus_.CurrentRound();
                if (round.proposed_block.header.height == msg.height && round.proposed_block.header.block_hash == msg.block_hash) {
                    // 区块已在 PRE_PREPARE 阶段验证过，直接提交
                    CommitBlockWithSignatures(round.proposed_block, {});
                    consensus_.ResetRound();
                    Logger::Info("backup committed block #" + std::to_string(msg.height));
                }
                // 生成本节点的 COMMIT 签名
                std::string my_sig = crypto::SignDetachedHex(SerializeConsensusMessageForSign(msg), consensus_.GetNodePrivateKey());
                ReplyJson(res, 200, Ok({{"accepted", true}, {"phase", "COMMIT"}, {"signature", my_sig}}));

            } else if (msg.type == "VIEW_CHANGE") {
                // ── 收到 VIEW_CHANGE ──
                bool quorum = false;
                uint64_t new_view = 0;
                consensus_.OnViewChange(msg, quorum, new_view);
                if (quorum) {
                    consensus_.AdvanceView();
                    // 如果我是新 Primary，立即尝试出块
                    if (consensus_.IsPrimary() && !mempool_.Pending().empty()) {
                        std::thread([this]() { TryProposeBlock(); }).detach();
                    }
                }
                ReplyJson(res, 200, Ok({{"accepted", true}, {"phase", "VIEW_CHANGE"}, {"quorum", quorum}}));

            } else {
                ReplyJson(res, 400, Err("unknown message type: " + msg.type));
            }
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

            // 检查 proposer 是否已被隔离
            if (consensus_.IsQuarantined(block.header.proposer_id)) {
                Logger::Warn("sync block #" + std::to_string(block.header.height) + " rejected: proposer " + block.header.proposer_id + " is quarantined");
                ReplyJson(res, 403, Err("proposer " + block.header.proposer_id + " is quarantined"));
                return;
            }

            // 检查高度是否连续
            uint64_t local_height = std::stoull(storage_.GetMetadata("latest_height", "0"));
            if (block.header.height <= local_height) {
                // 已有该区块, 忽略
                ReplyJson(res, 200, Ok({{"status", "IGNORED"}, {"reason", "already have height " + std::to_string(block.header.height)}}));
                return;
            }
            if (block.header.height > local_height + 1) {
                // 高度差距过大，触发区块追赶：从 proposer 的节点拉取缺失区块
                Logger::Info("height gap detected: local=" + std::to_string(local_height) + " incoming=" + std::to_string(block.header.height) + ", triggering catch-up");
                // 查找 proposer 对应的 peer 配置
                for (const auto& peer : config_.peers) {
                    if (peer.node_id == block.header.proposer_id) {
                        // 异步追赶，不阻塞当前请求
                        std::thread([this, peer, local_height, target = block.header.height]() {
                            CatchUpBlocks(peer, local_height + 1, target);
                        }).detach();
                        break;
                    }
                }
                ReplyJson(res, 200, Ok({{"status", "CATCH_UP_TRIGGERED"}, {"from", local_height + 1}, {"to", block.header.height}}));
                return;
            }

            // 验证前一区块哈希
            auto prev = storage_.GetLatestBlock();
            std::string expected_prev = prev ? prev->header.block_hash : std::string(64, '0');
            if (block.header.previous_block_hash != expected_prev) {
                ReplyJson(res, 400, Err("previous_block_hash mismatch"));
                return;
            }

            // 验证 Merkle 根
            std::string real_merkle = HashToHex(MerkleTree::ComputeRoot(block.transactions));
            if (real_merkle != block.header.tx_merkle_root) {
                Logger::Warn("sync block #" + std::to_string(block.header.height) + " rejected: merkle root mismatch, quarantining " + block.header.proposer_id);
                consensus_.QuarantineNode(block.header.proposer_id, "merkle_root_mismatch at height " + std::to_string(block.header.height));
                ReplyJson(res, 403, Err("merkle root mismatch: expected " + real_merkle));
                return;
            }

            // 验证 state_root
            std::string real_state = executor_.ExecuteForStateRoot(block.transactions);
            if (real_state != block.header.state_root) {
                Logger::Warn("sync block #" + std::to_string(block.header.height) + " rejected: state root mismatch, quarantining " + block.header.proposer_id);
                consensus_.QuarantineNode(block.header.proposer_id, "state_root_mismatch at height " + std::to_string(block.header.height));
                ReplyJson(res, 403, Err("state root mismatch: expected " + real_state));
                return;
            }

            // 验证 block_hash
            BlockHeader hdr = block.header;
            std::string saved_hash = hdr.block_hash;
            hdr.block_hash = "";
            if (ComputeBlockHash(hdr) != saved_hash) {
                Logger::Warn("sync block #" + std::to_string(block.header.height) + " rejected: block hash mismatch, quarantining " + block.header.proposer_id);
                consensus_.QuarantineNode(block.header.proposer_id, "block_hash_mismatch at height " + std::to_string(block.header.height));
                ReplyJson(res, 403, Err("block hash mismatch"));
                return;
            }

            // 全部验证通过，提交区块
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

    // 提供区块范围查询，供追赶同步使用
    server.Get("/p2p/sync/blocks", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            uint64_t from = 1;
            uint64_t to = std::stoull(storage_.GetMetadata("latest_height", "0"));
            if (req.has_param("from")) from = std::stoull(req.get_param_value("from"));
            if (req.has_param("to")) to = std::stoull(req.get_param_value("to"));
            if (from > to) { ReplyJson(res, 400, Err("from > to")); return; }
            if (to - from > 100) { ReplyJson(res, 400, Err("range too large (max 100)")); return; }

            nlohmann::json arr = nlohmann::json::array();
            for (uint64_t h = from; h <= to; ++h) {
                auto block = storage_.GetBlockByHeight(h);
                if (block) arr.push_back(BlockToJson(*block));
            }
            ReplyJson(res, 200, Ok({{"blocks", arr}, {"from", from}, {"to", to}}));
        } catch (const std::exception& e) {
            ReplyJson(res, 400, Err(e.what()));
        }
    });

    // 提供用户数据查询，供链重组时同步用户
    server.Get("/p2p/sync/users", [this](const httplib::Request&, httplib::Response& res) {
        auto users = storage_.GetAllUsers();
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& u : users) {
            arr.push_back({{"username", u.username}, {"password_hash", u.password_hash},
                           {"address", u.address}, {"public_key", u.public_key},
                           {"private_key_encrypted", u.private_key_encrypted}, {"created_at", u.created_at}});
        }
        ReplyJson(res, 200, Ok({{"users", arr}}));
    });

    // 手动触发区块追赶
    server.Post("/api/admin/sync", [this](const httplib::Request&, httplib::Response& res) {
        uint64_t local_height = std::stoull(storage_.GetMetadata("latest_height", "0"));
        // 查询所有 peer 的最高区块高度
        uint64_t max_height = local_height;
        std::string best_peer_id;
        for (const auto& peer : config_.peers) {
            if (consensus_.IsQuarantined(peer.node_id)) continue;
            try {
                httplib::Client client(peer.host, peer.rest_port);
                client.set_connection_timeout(2);
                client.set_read_timeout(3);
                auto r = client.Get("/api/node/status");
                if (r && r->status == 200) {
                    auto j = nlohmann::json::parse(r->body);
                    uint64_t h = std::stoull(j["data"].value("latest_height", "0"));
                    if (h > max_height) {
                        max_height = h;
                        best_peer_id = peer.node_id;
                    }
                }
            } catch (...) {}
        }
        if (max_height <= local_height) {
            ReplyJson(res, 200, Ok({{"status", "ALREADY_UP_TO_DATE"}, {"height", local_height}}));
            return;
        }
        // 找到最佳 peer 并触发追赶
        for (const auto& peer : config_.peers) {
            if (peer.node_id == best_peer_id) {
                Logger::Info("manual sync: catching up from " + std::to_string(local_height + 1) + " to " + std::to_string(max_height) + " via " + peer.node_id);
                std::thread([this, peer, local_height, max_height]() {
                    CatchUpBlocks(peer, local_height + 1, max_height);
                }).detach();
                ReplyJson(res, 200, Ok({{"status", "SYNC_STARTED"}, {"from", local_height + 1}, {"to", max_height}, {"via", best_peer_id}}));
                return;
            }
        }
        ReplyJson(res, 200, Ok({{"status", "NO_PEER_FOUND"}, {"height", local_height}}));
    });
}

void ApiServer::Run() {
    httplib::Server server;
    RegisterRoutes(server);
    Logger::Info("listening REST/P2P on port " + std::to_string(config_.rest_port));
    server.listen("0.0.0.0", config_.rest_port);
}

void ApiServer::BroadcastBlock(const Block& block) {
    // 异步广播区块到所有 peer 节点（跳过已隔离的节点）
    auto block_json = BlockToJson(block).dump();
    for (const auto& peer : config_.peers) {
        if (consensus_.IsQuarantined(peer.node_id)) {
            Logger::Info("skip broadcast to quarantined node " + peer.node_id);
            continue;
        }
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

void ApiServer::BroadcastBlockToPeer(const Block& block, size_t peer_idx) {
    // 向单个 peer 发送区块（用于 double_proposal 攻击模拟）
    if (peer_idx >= config_.peers.size()) return;
    const auto& peer = config_.peers[peer_idx];
    if (consensus_.IsQuarantined(peer.node_id)) return;
    auto block_json = BlockToJson(block).dump();
    std::thread([peer, block_json]() {
        try {
            httplib::Client client(peer.host, peer.rest_port);
            client.set_connection_timeout(3);
            client.set_read_timeout(5);
            auto res = client.Post("/p2p/sync/block", block_json, "application/json");
            if (res && res->status == 200) {
                Logger::Info("synced block to " + peer.node_id + " (selective)");
            } else {
                Logger::Warn("selective sync to " + peer.node_id + " failed: " + (res ? std::to_string(res->status) : "no response"));
            }
        } catch (const std::exception& e) {
            Logger::Warn("selective sync to " + peer.node_id + " exception: " + e.what());
        }
    }).detach();
}

void ApiServer::CatchUpBlocks(const PeerConfig& peer, uint64_t from_height, uint64_t target_height) {
    Logger::Info("catch-up: fetching blocks " + std::to_string(from_height) + " to " + std::to_string(target_height) + " from " + peer.node_id);

    httplib::Client client(peer.host, peer.rest_port);
    client.set_connection_timeout(5);
    client.set_read_timeout(10);

    // 检查是否需要链重组：比较本地和 peer 在 from_height 处的区块哈希
    bool need_reorg = false;
    auto local_block = storage_.GetBlockByHeight(from_height);
    if (local_block) {
        auto res = client.Get("/api/blocks/" + std::to_string(from_height));
        if (res && res->status == 200) {
            auto peer_block = BlockFromJson(nlohmann::json::parse(res->body)["data"]);
            if (peer_block.header.block_hash != local_block->header.block_hash) {
                need_reorg = true;
                Logger::Info("catch-up: chain fork detected at height " + std::to_string(from_height) + ", will reorg");
            }
        }
    }

    if (need_reorg) {
        // 链重组：需要从 height=1 开始重建。
        // 清空 SMT 和区块表，重新拉取并应用所有区块。
        Logger::Info("catch-up: starting chain reorg from height 1 to " + std::to_string(target_height));
        executor_.ResetSMT();

        // 从 peer 批量拉取区块
        std::vector<Block> peer_blocks;
        for (uint64_t h = 1; h <= target_height; ++h) {
            auto res = client.Get("/api/blocks/" + std::to_string(h));
            if (!res || res->status != 200) {
                Logger::Warn("catch-up: reorg failed: cannot fetch block #" + std::to_string(h) + " from " + peer.node_id);
                return;
            }
            peer_blocks.push_back(BlockFromJson(nlohmann::json::parse(res->body)["data"]));
        }

        // 验证 peer 区块链内部连续性
        for (size_t i = 0; i < peer_blocks.size(); ++i) {
            const auto& block = peer_blocks[i];
            std::string expected_prev = (i == 0) ? std::string(64, '0') : peer_blocks[i - 1].header.block_hash;
            if (block.header.previous_block_hash != expected_prev) {
                Logger::Warn("catch-up: reorg failed: peer chain not internally consistent at height " + std::to_string(block.header.height));
                consensus_.QuarantineNode(peer.node_id, "catch-up peer chain inconsistent at height " + std::to_string(block.header.height));
                return;
            }
            // 验证 Merkle 根
            std::string real_merkle = HashToHex(MerkleTree::ComputeRoot(block.transactions));
            if (real_merkle != block.header.tx_merkle_root) {
                Logger::Warn("catch-up: reorg failed: merkle mismatch at height " + std::to_string(block.header.height));
                consensus_.QuarantineNode(peer.node_id, "catch-up merkle mismatch at height " + std::to_string(block.header.height));
                return;
            }
            // 验证 block_hash
            BlockHeader hdr = block.header;
            std::string saved_hash = hdr.block_hash;
            hdr.block_hash = "";
            if (ComputeBlockHash(hdr) != saved_hash) {
                Logger::Warn("catch-up: reorg failed: hash mismatch at height " + std::to_string(block.header.height));
                consensus_.QuarantineNode(peer.node_id, "catch-up hash mismatch at height " + std::to_string(block.header.height));
                return;
            }
        }

        // 全部验证通过，清空旧区块数据并重新应用
        storage_.ClearChainData();
        storage_.PutMetadata("latest_height", "0");

        // 同步用户数据（用户注册是链下操作，不包含在区块中）
        auto users_res = client.Get("/p2p/sync/users");
        if (users_res && users_res->status == 200) {
            try {
                auto users_json = nlohmann::json::parse(users_res->body)["data"]["users"];
                for (const auto& u : users_json) {
                    storage_.PutUser(u["username"].get<std::string>(),
                                     u["password_hash"].get<std::string>(),
                                     u["address"].get<std::string>(),
                                     u["public_key"].get<std::string>(),
                                     u.value("private_key_encrypted", ""),
                                     u.value("created_at", 0));
                }
                Logger::Info("catch-up: synced " + std::to_string(users_json.size()) + " users from " + peer.node_id);
            } catch (const std::exception& e) {
                Logger::Warn("catch-up: failed to sync users: " + std::string(e.what()));
            }
        }

        for (const auto& block : peer_blocks) {
            // 逐块提交，跳过 state_root 验证（SMT 和账户状态正在重建中）
            executor_.CommitBlock(block);
            Logger::Info("catch-up: reorg applied block #" + std::to_string(block.header.height));
            consensus_.AddEvent({0, 0, "", block.header.height, block.header.view, block.header.instance_id,
                                 "CATCH_UP_REORG", peer.node_id, config_.node_id, block.header.block_hash, true, "", ""});
        }
        uint64_t final_height = std::stoull(storage_.GetMetadata("latest_height", "0"));
        Logger::Info("catch-up: reorg complete, now at height " + std::to_string(final_height));
        consensus_.AddEvent({0, 0, "", final_height, 0, 0, "CATCH_UP_COMPLETE", peer.node_id, config_.node_id, "", true, "chain reorg", ""});
        return;
    }

    // 简单追赶（链未分叉，只是高度落后）：逐块拉取并应用
    std::string prev_hash = std::string(64, '0');
    if (from_height > 1) {
        auto local_prev = storage_.GetBlockByHeight(from_height - 1);
        if (local_prev) prev_hash = local_prev->header.block_hash;
    }

    for (uint64_t h = from_height; h <= target_height; ++h) {
        auto existing = storage_.GetBlockByHeight(h);
        if (existing) {
            prev_hash = existing->header.block_hash;
            continue;
        }

        auto res = client.Get("/api/blocks/" + std::to_string(h));
        if (!res || res->status != 200) {
            Logger::Warn("catch-up: failed to fetch block #" + std::to_string(h) + " from " + peer.node_id);
            return;
        }

        try {
            Block block = BlockFromJson(nlohmann::json::parse(res->body)["data"]);

            // 验证 prev_hash 连续性
            if (block.header.previous_block_hash != prev_hash) {
                Logger::Warn("catch-up: block #" + std::to_string(h) + " prev_hash mismatch, need full reorg");
                // 触发完整重新同步
                CatchUpBlocks(peer, 1, target_height);
                return;
            }

            // 验证 Merkle 根
            std::string real_merkle = HashToHex(MerkleTree::ComputeRoot(block.transactions));
            if (real_merkle != block.header.tx_merkle_root) {
                Logger::Warn("catch-up: block #" + std::to_string(h) + " merkle mismatch");
                consensus_.QuarantineNode(peer.node_id, "catch-up merkle mismatch at height " + std::to_string(h));
                return;
            }

            // 注意：跳过 state_root 验证。追赶同步时本地账户状态可能不完整（缺少初始余额），
            // 无法独立重算 state_root。Merkle 根和区块哈希验证已保证区块内容完整性。

            // 验证 block_hash
            BlockHeader hdr = block.header;
            std::string saved_hash = hdr.block_hash;
            hdr.block_hash = "";
            if (ComputeBlockHash(hdr) != saved_hash) {
                Logger::Warn("catch-up: block #" + std::to_string(h) + " hash mismatch");
                consensus_.QuarantineNode(peer.node_id, "catch-up hash mismatch at height " + std::to_string(h));
                return;
            }

            executor_.CommitBlock(block);
            prev_hash = block.header.block_hash;
            Logger::Info("catch-up: applied block #" + std::to_string(h));
            consensus_.AddEvent({0, 0, "", h, block.header.view, block.header.instance_id,
                                 "CATCH_UP_BLOCK", peer.node_id, config_.node_id, block.header.block_hash, true, "", ""});
        } catch (const std::exception& e) {
            Logger::Warn("catch-up: exception at block #" + std::to_string(h) + ": " + e.what());
            return;
        }
    }
    // 同步用户数据（无论简单追赶还是重组都需要）
    auto users_res = client.Get("/p2p/sync/users");
    if (users_res && users_res->status == 200) {
        try {
            auto users_json = nlohmann::json::parse(users_res->body)["data"]["users"];
            for (const auto& u : users_json) {
                storage_.PutUser(u["username"].get<std::string>(),
                                 u["password_hash"].get<std::string>(),
                                 u["address"].get<std::string>(),
                                 u["public_key"].get<std::string>(),
                                 u.value("private_key_encrypted", ""),
                                 u.value("created_at", 0));
            }
            Logger::Info("catch-up: synced " + std::to_string(users_json.size()) + " users from " + peer.node_id);
        } catch (const std::exception& e) {
            Logger::Warn("catch-up: failed to sync users: " + std::string(e.what()));
        }
    }

    uint64_t final_height = std::stoull(storage_.GetMetadata("latest_height", "0"));
    Logger::Info("catch-up: complete, now at height " + std::to_string(final_height));
    consensus_.AddEvent({0, 0, "", final_height, 0, 0, "CATCH_UP_COMPLETE", peer.node_id, config_.node_id, "", true, "", ""});
}

// ══════════════════════════════════════════════════════════════════════════════
// PBFT 共识消息广播
// ══════════════════════════════════════════════════════════════════════════════

void ApiServer::BroadcastConsensusMessage(const ConsensusMessage& msg) {
    // 同步广播（httplib handler 已在独立线程中运行）
    auto msg_json = ConsensusMessageToJson(msg).dump();
    for (const auto& peer : config_.peers) {
        if (consensus_.IsQuarantined(peer.node_id)) continue;
        try {
            httplib::Client client(peer.host, peer.rest_port);
            client.set_connection_timeout(2);
            client.set_read_timeout(5);
            auto res = client.Post("/p2p/consensus/message", msg_json, "application/json");
            if (res && res->status == 200) {
                Logger::Info("sent " + msg.type + " to " + peer.node_id + ": OK");
            }
        } catch (...) {}
    }
}

// ══════════════════════════════════════════════════════════════════════════════
// PBFT 共识定时器
// ══════════════════════════════════════════════════════════════════════════════

void ApiServer::ConsensusTimerLoop() {
    while (!stop_timer_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));

        if (!consensus_.IsRunning()) continue;
        if (!storage_.Raw()) continue;  // DB 未初始化

        // 检查是否需要 View Change（当前轮次超时）
        if (consensus_.ShouldViewChange()) {
            Logger::Warn("consensus timeout, triggering view change");
            auto vc = consensus_.CreateViewChange();
            BroadcastConsensusMessage(vc);
            // 自己也处理
            bool quorum = false;
            uint64_t new_view = 0;
            consensus_.OnViewChange(vc, quorum, new_view);
            if (quorum) {
                consensus_.AdvanceView();
            }
            continue;
        }

        // 如果我是 Primary 且 mempool 有交易且当前没有进行中的轮次
        // 在新线程中出块，避免阻塞定时器
        if (consensus_.IsPrimary() && consensus_.CurrentRound().phase == ConsensusPhase::IDLE) {
            if (!mempool_.Pending().empty()) {
                std::thread([this]() { TryProposeBlock(); }).detach();
            }
        }
    }
}

void ApiServer::TryProposeBlock() {
    try {
        auto picked = mempool_.PickTransactions(100);
        if (picked.empty()) return;

        // 过滤掉无法执行的交易（余额不足等）
        std::vector<Transaction> valid_txs;
        for (const auto& tx : picked) {
            try {
                std::vector<Transaction> single{tx};
                executor_.ExecuteForStateRoot(single);
                valid_txs.push_back(tx);
            } catch (const std::exception& e) {
                Logger::Warn("dropping invalid tx " + tx.tx_id.substr(0, 16) + ": " + e.what());
                mempool_.RemoveCommitted({tx});
            }
        }
        if (valid_txs.empty()) return;

        Block block;
        block.transactions = valid_txs;
        block.header.chain_id = config_.chain_id;
        block.header.height = std::stoull(storage_.GetMetadata("latest_height", "0")) + 1;
        auto latest = storage_.GetLatestBlock();
        block.header.previous_block_hash = latest ? latest->header.block_hash : std::string(64, '0');
        block.header.tx_merkle_root = HashToHex(MerkleTree::ComputeRoot(picked));
        block.header.state_root = executor_.ExecuteForStateRoot(picked);
        block.header.timestamp = NowMillis();
        block.header.view = consensus_.CurrentView();
        block.header.instance_id = 0;
        block.header.proposer_id = config_.node_id;
        block.header.block_hash = ComputeBlockHash(block.header);

        // 攻击模式
        auto mode = consensus_.GetAttackMode();
        if (mode == AttackMode::BAD_MERKLE_ROOT) {
            block.header.tx_merkle_root = std::string(64, '0');
            Logger::Warn("attack: tampered merkle root on block #" + std::to_string(block.header.height));
        } else if (mode == AttackMode::BAD_STATE_ROOT) {
            block.header.state_root = std::string(64, '0');
            Logger::Warn("attack: tampered state root on block #" + std::to_string(block.header.height));
        } else if (mode == AttackMode::INVALID_BLOCK_HASH) {
            block.header.block_hash = std::string(64, '0');
            Logger::Warn("attack: tampered block hash on block #" + std::to_string(block.header.height));
        } else if (mode == AttackMode::DOUBLE_PROPOSAL) {
            // 发送两个不同的 PRE_PREPARE
            Block tampered = block;
            tampered.header.state_root = std::string(64, '0');
            tampered.header.block_hash = ComputeBlockHash(tampered.header);
            Logger::Warn("attack: double_proposal on block #" + std::to_string(block.header.height));
            auto normal_msg = consensus_.CreatePrePrepare(block);
            auto tampered_msg = consensus_.CreatePrePrepare(tampered);
            // 正常消息发给第一个 peer，篡改消息发给其余 peer
            if (!config_.peers.empty()) {
                auto normal_json = ConsensusMessageToJson(normal_msg).dump();
                auto tampered_json = ConsensusMessageToJson(tampered_msg).dump();
                for (size_t i = 0; i < config_.peers.size(); ++i) {
                    const auto& peer = config_.peers[i];
                    auto& json = (i == 0) ? normal_json : tampered_json;
                    std::thread([peer, json]() {
                        try {
                            httplib::Client client(peer.host, peer.rest_port);
                            client.set_connection_timeout(3);
                            client.set_read_timeout(5);
                            client.Post("/p2p/consensus/message", json, "application/json");
                        } catch (...) {}
                    }).detach();
                }
            }
            return;
        }

        // 创建 PRE_PREPARE
        auto pre_prepare = consensus_.CreatePrePrepare(block);
        auto pre_prepare_json = ConsensusMessageToJson(pre_prepare).dump();

        // Phase 1: 发送 PRE_PREPARE 给所有备份节点，统计接受数
        int prepare_votes = 1;  // 自己算一票
        for (const auto& peer : config_.peers) {
            if (consensus_.IsQuarantined(peer.node_id)) continue;
            try {
                httplib::Client client(peer.host, peer.rest_port);
                client.set_connection_timeout(2);
                client.set_read_timeout(5);
                auto res = client.Post("/p2p/consensus/message", pre_prepare_json, "application/json");
                if (res && res->status == 200) {
                    prepare_votes++;
                    Logger::Info("PRE_PREPARE accepted by " + peer.node_id);
                } else {
                    Logger::Warn("PRE_PREPARE rejected by " + peer.node_id + ": " + (res ? std::to_string(res->status) : "no response"));
                }
            } catch (const std::exception& e) {
                Logger::Warn("PRE_PREPARE send to " + peer.node_id + " failed: " + e.what());
            }
        }

        Logger::Info("PRE_PREPARE votes: " + std::to_string(prepare_votes) + "/" + std::to_string(consensus_.Quorum()));

        if (prepare_votes < consensus_.Quorum()) {
            Logger::Warn("insufficient PREPARE votes, aborting");
            consensus_.ResetRound();
            return;
        }

        // Phase 2: 发送 COMMIT 给所有备份节点
        ConsensusMessage commit_msg;
        commit_msg.msg_id = config_.node_id + ":" + std::to_string(block.header.height) + ":" + std::to_string(consensus_.CurrentView()) + ":commit";
        commit_msg.type = "COMMIT";
        commit_msg.chain_id = config_.chain_id;
        commit_msg.height = block.header.height;
        commit_msg.view = consensus_.CurrentView();
        commit_msg.instance_id = 0;
        commit_msg.block_hash = block.header.block_hash;
        commit_msg.sender_id = config_.node_id;
        commit_msg.timestamp = NowMillis();
        commit_msg.signature_hex = crypto::SignDetachedHex(SerializeConsensusMessageForSign(commit_msg), consensus_.GetNodePrivateKey());
        auto commit_json = ConsensusMessageToJson(commit_msg).dump();

        int commit_votes = 1;  // 自己算一票
        std::vector<NodeSignature> sigs;
        NodeSignature self_sig;
        self_sig.node_id = config_.node_id;
        self_sig.signature_hex = commit_msg.signature_hex;
        sigs.push_back(self_sig);

        for (const auto& peer : config_.peers) {
            if (consensus_.IsQuarantined(peer.node_id)) continue;
            try {
                httplib::Client client(peer.host, peer.rest_port);
                client.set_connection_timeout(2);
                client.set_read_timeout(5);
                auto res = client.Post("/p2p/consensus/message", commit_json, "application/json");
                if (res && res->status == 200) {
                    commit_votes++;
                    // 收集签名
                    NodeSignature sig;
                    sig.node_id = peer.node_id;
                    try {
                        auto body = nlohmann::json::parse(res->body);
                        if (body.contains("data") && body["data"].contains("signature")) {
                            sig.signature_hex = body["data"]["signature"].get<std::string>();
                        }
                    } catch (...) {}
                    sigs.push_back(sig);
                    Logger::Info("COMMIT accepted by " + peer.node_id);
                }
            } catch (...) {}
        }

        Logger::Info("COMMIT votes: " + std::to_string(commit_votes) + "/" + std::to_string(consensus_.Quorum()));

        if (commit_votes < consensus_.Quorum()) {
            Logger::Warn("insufficient COMMIT votes, aborting");
            consensus_.ResetRound();
            return;
        }

        // Phase 3: 提交区块
        CommitBlockWithSignatures(block, sigs);
        consensus_.ResetRound();

        Logger::Info("proposed and committed block #" + std::to_string(block.header.height) + " hash=" + block.header.block_hash.substr(0, 16) + "...");
    } catch (const std::exception& e) {
        Logger::Warn("propose block failed: " + std::string(e.what()));
    }
}

void ApiServer::CommitBlockWithSignatures(const Block& block, const std::vector<NodeSignature>& sigs) {
    // 验证 state_root
    std::string real_state = executor_.ExecuteForStateRoot(block.transactions);
    if (real_state != block.header.state_root) {
        Logger::Warn("commit rejected: state_root mismatch at height " + std::to_string(block.header.height));
        return;
    }

    // 提交区块（带签名）
    Block committed_block = block;
    committed_block.commit_signatures = sigs;
    executor_.CommitBlock(committed_block);
    mempool_.RemoveCommitted(block.transactions);

    Logger::Info("committed block #" + std::to_string(block.header.height) + " with " + std::to_string(sigs.size()) + " signatures");
    consensus_.AddEvent({0, 0, "", block.header.height, block.header.view, block.header.instance_id,
                         "BLOCK_COMMITTED", block.header.proposer_id, config_.node_id, block.header.block_hash,
                         true, "sigs=" + std::to_string(sigs.size()), ""});
}

} // namespace rbft
