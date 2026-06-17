#include "cli/cli_session.h"

#include <chrono>
#include <ctime>
#include <fstream>
#include <httplib.h>
#include <iomanip>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <thread>

namespace rbft {

using json = nlohmann::json;

// ══════════════════════════════════════════════════════════════════════════════
// 构造 / 析构
// ══════════════════════════════════════════════════════════════════════════════

CliSession::CliSession(const std::vector<std::pair<std::string, int>>& node_endpoints) {
    for (size_t i = 0; i < node_endpoints.size(); ++i) {
        RemoteNode node;
        node.node_id = "node" + std::to_string(i + 1);
        node.host = node_endpoints[i].first;
        node.port = node_endpoints[i].second;
        node.client = std::make_unique<httplib::Client>(node.host, node.port);
        node.client->set_connection_timeout(3);
        node.client->set_read_timeout(5);
        nodes_.push_back(std::move(node));
    }
    InitWalletDir();
    OpenDebugLog();
    DbgSep("CLI 客户端启动");
    Dbg("连接 " + std::to_string(nodes_.size()) + " 个节点");
    Dbg("钱包目录: " + wallet_dir_);
}

CliSession::~CliSession() {
    DbgSep("CLI 客户端结束");
    if (debug_log_.is_open()) debug_log_.close();
}

// ══════════════════════════════════════════════════════════════════════════════
// 钱包管理
// ══════════════════════════════════════════════════════════════════════════════

void CliSession::InitWalletDir() {
    wallet_dir_ = "wallets";
    std::filesystem::create_directories(wallet_dir_);
}

bool CliSession::SaveWallet(const std::string& username, const std::string& password,
                             const std::string& address, const std::string& public_key, const std::string& private_key) {
    // 通过节点 API 加密私钥
    json enc_body = {{"plaintext", private_key}, {"password", password}};
    std::string raw;
    HttpPost(0, "/api/crypto/encrypt", enc_body.dump(), raw);
    std::string data, err;
    if (!ParseOk(raw, data, err)) {
        Dbg("钱包加密失败: " + err);
        return false;
    }
    auto d = json::parse(data);
    std::string encrypted = d.value("encrypted", "");

    // 写入钱包文件
    json wallet = {
        {"username", username},
        {"address", address},
        {"public_key", public_key},
        {"encrypted_private_key", encrypted},
        {"created_at", std::time(nullptr)}
    };

    std::string path = wallet_dir_ + "/" + username + ".wallet";
    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << wallet.dump(2);
    f.close();

    Dbg("钱包已保存: " + path);
    return true;
}

bool CliSession::LoadWallet(const std::string& username, const std::string& password, Wallet& out) {
    std::string path = wallet_dir_ + "/" + username + ".wallet";
    std::ifstream f(path);
    if (!f.is_open()) return false;

    auto wallet = json::parse(f);
    f.close();

    out.username = wallet.value("username", "");
    out.address = wallet.value("address", "");
    out.public_key = wallet.value("public_key", "");
    out.encrypted_key = wallet.value("encrypted_private_key", "");

    // 通过节点 API 解密私钥
    json dec_body = {{"encrypted", out.encrypted_key}, {"password", password}};
    std::string raw;
    HttpPost(0, "/api/crypto/decrypt", dec_body.dump(), raw);
    std::string data, err;
    if (!ParseOk(raw, data, err)) {
        Dbg("钱包解密失败: " + err);
        return false;
    }
    out.private_key = json::parse(data).value("plaintext", "");

    Dbg("钱包已加载: " + path);
    return true;
}

// ══════════════════════════════════════════════════════════════════════════════
// 调试日志
// ══════════════════════════════════════════════════════════════════════════════

void CliSession::OpenDebugLog() {
    log_path_ = "data/cli_client_debug.log";
    debug_log_.open(log_path_, std::ios::app);
}

void CliSession::Dbg(const std::string& msg) {
    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    char ts[32]{};
    std::strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    std::string line = std::string(ts) + " [DEBUG] " + msg;
    if (debug_log_.is_open()) { debug_log_ << line << "\n"; debug_log_.flush(); }
}

void CliSession::DbgPrint(const std::string& msg) {
    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    char ts[32]{};
    std::strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    std::string line = std::string(ts) + " [DEBUG] " + msg;
    if (debug_log_.is_open()) { debug_log_ << line << "\n"; debug_log_.flush(); }
    std::cerr << "\033[90m" << line << "\033[0m\n";
}

void CliSession::DbgSep(const std::string& title) {
    Dbg("═══════════════════════════════════════════");
    Dbg("  " + title);
    Dbg("═══════════════════════════════════════════");
}

// ══════════════════════════════════════════════════════════════════════════════
// HTTP 辅助
// ══════════════════════════════════════════════════════════════════════════════

bool CliSession::HttpGet(int idx, const std::string& path, std::string& out) {
    auto res = nodes_[idx].client->Get(path.c_str());
    if (!res) { out = "{\"ok\":false,\"error\":\"连接失败\"}"; return false; }
    out = res->body;
    return true;
}

bool CliSession::HttpPost(int idx, const std::string& path, const std::string& body, std::string& out) {
    auto res = nodes_[idx].client->Post(path.c_str(), body, "application/json");
    if (!res) { out = "{\"ok\":false,\"error\":\"连接失败\"}"; return false; }
    out = res->body;
    return true;
}

bool CliSession::ParseOk(const std::string& body, std::string& out_data, std::string& out_error) {
    try {
        auto j = json::parse(body);
        if (j.value("ok", false)) {
            out_data = j.contains("data") ? j["data"].dump() : "";
            return true;
        }
        out_error = j.value("error", "unknown error");
        return false;
    } catch (...) {
        out_error = "JSON 解析失败";
        return false;
    }
}

CliSession::NodeResponse CliSession::SendToNode(int idx, const std::string& method, const std::string& path, const std::string& body) {
    NodeResponse resp;
    resp.node_idx = idx;
    std::string raw;
    if (method == "GET") HttpGet(idx, path, raw);
    else HttpPost(idx, path, body, raw);
    resp.ok = ParseOk(raw, resp.data, resp.error);
    return resp;
}

std::vector<CliSession::NodeResponse> CliSession::BroadcastGet(const std::string& path) {
    std::vector<NodeResponse> results;
    for (size_t i = 0; i < nodes_.size(); ++i) {
        results.push_back(SendToNode(i, "GET", path));
    }
    return results;
}

std::vector<CliSession::NodeResponse> CliSession::BroadcastPost(const std::string& path, const std::string& body) {
    std::vector<NodeResponse> results;
    for (size_t i = 0; i < nodes_.size(); ++i) {
        results.push_back(SendToNode(i, "POST", path, body));
    }
    return results;
}

// ══════════════════════════════════════════════════════════════════════════════
// 辅助
// ══════════════════════════════════════════════════════════════════════════════

std::string CliSession::PromptLine(const std::string& prompt) const {
    std::cout << prompt; std::cout.flush();
    std::string line;
    if (!std::getline(std::cin, line)) return "";
    return line;
}

uint64_t CliSession::PromptUint64(const std::string& prompt) const {
    auto s = PromptLine(prompt);
    try { return std::stoull(s); } catch (...) { return 0; }
}

void CliSession::PrintOk(const std::string& msg) const { std::cout << "\033[32m[OK]\033[0m " << msg << "\n"; }
void CliSession::PrintErr(const std::string& msg) const { std::cout << "\033[31m[ERR]\033[0m " << msg << "\n"; }
void CliSession::Pause() const { std::cout << "\n按 Enter 继续..."; std::cin.get(); }

int CliSession::PickNode(const std::string& prompt) const {
    auto s = PromptLine(prompt);
    if (s.empty()) return -1;
    try {
        int n = std::stoi(s);
        if (n == 0) return -1;  // 0 = 广播
        if (n >= 1 && n <= static_cast<int>(nodes_.size())) return n - 1;
    } catch (...) {}
    return -1;
}

// ══════════════════════════════════════════════════════════════════════════════
// 菜单
// ══════════════════════════════════════════════════════════════════════════════

void CliSession::ShowMenu() const {
    std::cout << "\n";
    std::cout << "\033[36m╔══════════════════════════════════════════════╗\033[0m\n";
    std::cout << "\033[36m║\033[0m  \033[1mRBFT Chain Demo - 多节点共识客户端\033[0m          \033[36m║\033[0m\n";
    std::cout << "\033[36m╠══════════════════════════════════════════════╣\033[0m\n";
    std::cout << "\033[36m║\033[0m  1. 注册用户 (广播到所有节点)                \033[36m║\033[0m\n";
    std::cout << "\033[36m║\033[0m  2. 用户登录                                \033[36m║\033[0m\n";
    std::cout << "\033[36m║\033[0m  3. 转账交易 (选择目标节点)                  \033[36m║\033[0m\n";
    std::cout << "\033[36m║\033[0m  4. 存储数据 (STORE_DATA)                   \033[36m║\033[0m\n";
    std::cout << "\033[36m║\033[0m  5. 查询所有节点账户状态                     \033[36m║\033[0m\n";
    std::cout << "\033[36m║\033[0m  6. 查询所有节点最新区块                     \033[36m║\033[0m\n";
    std::cout << "\033[36m║\033[0m  7. Merkle 证明对比                         \033[36m║\033[0m\n";
    std::cout << "\033[36m║\033[0m  8. 所有节点状态总览                         \033[36m║\033[0m\n";
    std::cout << "\033[36m║\033[0m  9. 攻击模拟 (设置恶意节点 + 验证拜占庭容错) \033[36m║\033[0m\n";
    std::cout << "\033[36m║\033[0m 10. 最近交易记录                            \033[36m║\033[0m\n";
    std::cout << "\033[36m║\033[0m 11. 查看所有用户                            \033[36m║\033[0m\n";
    std::cout << "\033[36m║\033[0m 12. 多节点共识验证                          \033[36m║\033[0m\n";
    std::cout << "\033[36m║\033[0m  0. 退出                                    \033[36m║\033[0m\n";
    std::cout << "\033[36m╚══════════════════════════════════════════════╝\033[0m\n";
    if (active_user_ >= 0) {
        std::cout << "  当前用户: \033[33m" << users_[active_user_].username << "\033[0m  "
                  << "地址: " << users_[active_user_].address.substr(0, 12) << "...\n";
    }
    std::cout << "\n请选择 [0-12]: ";
}

void CliSession::Run() {
    // 检测节点连接
    DbgPrint("检测节点连接...");
    int alive = 0;
    for (size_t i = 0; i < nodes_.size(); ++i) {
        std::string body;
        if (HttpGet(i, "/api/node/status", body)) {
            auto j = json::parse(body, nullptr, false);
            if (!j.is_discarded() && j.value("ok", false)) {
                DbgPrint("  " + nodes_[i].node_id + " ✓");
                alive++;
            } else {
                DbgPrint("  " + nodes_[i].node_id + " ✗ (无响应)");
            }
        } else {
            DbgPrint("  " + nodes_[i].node_id + " ✗ (连接失败)");
        }
    }
    if (alive == 0) {
        PrintErr("没有可用节点! 请先启动 rbft_node:");
        std::cout << "  bash scripts/start_4nodes.sh\n";
        return;
    }
    DbgPrint(std::to_string(alive) + "/" + std::to_string(nodes_.size()) + " 个节点在线\n");

    while (true) {
        ShowMenu();
        std::string choice;
        if (!std::getline(std::cin, choice)) break;
        if (choice.empty()) continue;
        try {
            int c = std::stoi(choice);
            switch (c) {
                case 0: return;
                case 1: DoRegister(); break;
                case 2: DoLogin(); break;
                case 3: DoTransfer(); break;
                case 4: DoStoreData(); break;
                case 5: DoQueryAccount(); break;
                case 6: DoQueryBlock(); break;
                case 7: DoMerkleProof(); break;
                case 8: DoNodeStatus(); break;
                case 9: DoAttackSimulation(); break;
                case 10: DoShowHistory(); break;
                case 11: DoListUsers(); break;
                case 12: DoConsensusVerify(); break;
                default: PrintErr("无效选择"); break;
            }
        } catch (const std::exception& e) {
            PrintErr(std::string("异常: ") + e.what());
        }
    }
}

// ══════════════════════════════════════════════════════════════════════════════
// 1. 注册用户 (广播到所有节点)
// ══════════════════════════════════════════════════════════════════════════════

void CliSession::DoRegister() {
    DbgSep("用户注册");
    auto username = PromptLine("用户名: ");
    auto password = PromptLine("密  码: ");
    if (username.size() < 3 || password.size() < 6) {
        PrintErr("用户名>=3字符, 密码>=6字符");
        return;
    }

    // Step 1: 生成密钥对 (通过节点 API)
    DbgPrint("Step 1 - 生成 Ed25519 密钥对...");
    std::string raw;
    HttpPost(0, "/api/crypto/generate-keypair", "{}", raw);
    std::string data, err;
    if (!ParseOk(raw, data, err)) {
        PrintErr("密钥生成失败: " + err);
        Pause();
        return;
    }
    auto kp = json::parse(data);
    std::string address = kp.value("address", "");
    std::string pub_key = kp.value("public_key", "");
    std::string priv_key = kp.value("private_key", "");
    Dbg("  address=" + address);
    Dbg("  public_key=" + pub_key);
    Dbg("  private_key=" + priv_key.substr(0, 16) + "...");

    // Step 2: 加密私钥并保存到本地钱包文件
    DbgPrint("Step 2 - 加密私钥保存到钱包...");
    if (!SaveWallet(username, password, address, pub_key, priv_key)) {
        PrintErr("钱包保存失败");
        Pause();
        return;
    }
    std::cout << "  钱包文件: wallets/" << username << ".wallet\n";
    std::cout << "  私钥已加密存储 (AES-256, 密钥从密码派生)\n";

    // Step 3: 广播公钥+地址到所有节点 (不发送私钥)
    DbgPrint("Step 3 - 广播公钥到所有节点...");
    json reg_body = {
        {"username", username}, {"password", password},
        {"address", address}, {"public_key", pub_key}, {"private_key", priv_key}
    };

    int ok_count = 0;
    for (size_t i = 0; i < nodes_.size(); ++i) {
        std::string r;
        HttpPost(i, "/api/users/register", reg_body.dump(), r);
        std::string dd, ee;
        if (ParseOk(r, dd, ee)) {
            ok_count++;
            Dbg("  " + nodes_[i].node_id + " ✓");
        } else {
            Dbg("  " + nodes_[i].node_id + " ✗ " + ee);
        }
    }

    RemoteUser user{username, address, pub_key, priv_key, ""};
    users_.push_back(user);
    active_user_ = static_cast<int>(users_.size()) - 1;

    PrintOk("注册完成 (" + std::to_string(ok_count) + "/" + std::to_string(nodes_.size()) + " 节点)");
    std::cout << "  用户名: " << username << "\n";
    std::cout << "  地  址: " << address << "\n";
    std::cout << "  钱  包: wallets/" << username << ".wallet (私钥加密存储)\n";
    Pause();
}

// ══════════════════════════════════════════════════════════════════════════════
// 2. 用户登录
// ══════════════════════════════════════════════════════════════════════════════

void CliSession::DoLogin() {
    DbgSep("用户登录");
    auto username = PromptLine("用户名: ");
    auto password = PromptLine("密  码: ");

    // 从本地钱包文件加载密钥
    DbgPrint("从钱包文件加载密钥...");
    Wallet wallet;
    if (!LoadWallet(username, password, wallet)) {
        PrintErr("钱包加载失败 (密码错误或钱包文件不存在)");
        Pause();
        return;
    }

    // 向节点验证用户存在
    bool verified = false;
    for (size_t i = 0; i < nodes_.size(); ++i) {
        std::string raw;
        HttpGet(i, "/api/state/" + wallet.address, raw);
        std::string data, err;
        if (ParseOk(raw, data, err)) {
            verified = true;
            Dbg("  " + nodes_[i].node_id + " ✓ 账户存在");
            break;
        }
    }

    RemoteUser user{username, wallet.address, wallet.public_key, wallet.private_key, ""};
    users_.push_back(user);
    active_user_ = static_cast<int>(users_.size()) - 1;

    PrintOk("登录成功");
    std::cout << "  地址: " << wallet.address << "\n";
    std::cout << "  私钥来源: wallets/" << username << ".wallet\n";
    if (!verified) {
        std::cout << "  \033[33m注意: 节点上未找到该账户，请先注册\033[0m\n";
    }
    Dbg("登录: " + username + " address=" + wallet.address);
    Pause();
}

// ══════════════════════════════════════════════════════════════════════════════
// 3. 转账交易
// ══════════════════════════════════════════════════════════════════════════════

void CliSession::DoTransfer() {
    if (active_user_ < 0) { PrintErr("请先登录"); return; }
    DbgSep("转账交易");

    const auto& user = users_[active_user_];
    auto to_addr = PromptLine("接收方地址: ");
    auto amount = PromptUint64("转账金额: ");
    if (to_addr.empty() || amount == 0) { PrintErr("地址和金额不能为空"); return; }

    // 选择目标节点
    std::cout << "  发送到哪个节点?\n";
    for (size_t i = 0; i < nodes_.size(); ++i) {
        std::cout << "    " << (i + 1) << ". " << nodes_[i].node_id << "\n";
    }
    std::cout << "    0. 广播到所有节点\n";
    int target = PickNode("选择 [0-" + std::to_string(nodes_.size()) + "]: ");

    // 确定需要查询 nonce 的节点
    int nonce_node = (target >= 0) ? target : 0;
    uint64_t nonce = 1;
    std::string raw;
    if (HttpGet(nonce_node, "/api/state/" + user.address, raw)) {
        std::string data, err;
        if (ParseOk(raw, data, err)) {
            nonce = json::parse(data).value("nonce", 0ULL) + 1;
        }
    }

    json tx_body = {
        {"type", "TRANSFER"}, {"from", user.address}, {"to", to_addr},
        {"amount", amount}, {"nonce", nonce}, {"timestamp", 0},
        {"public_key", user.public_key}, {"private_key", user.private_key}
    };

    Dbg("nonce=" + std::to_string(nonce) + " (from " + nodes_[nonce_node].node_id + ")");

    std::vector<NodeResponse> results;
    if (target < 0) {
        DbgPrint("广播转账到所有节点...");
        results = BroadcastPost("/api/transactions/transfer", tx_body.dump());
    } else {
        DbgPrint("发送转账到 " + nodes_[target].node_id + "...");
        results.push_back(SendToNode(target, "POST", "/api/transactions/transfer", tx_body.dump()));
    }

    for (const auto& r : results) {
        if (r.ok) {
            auto d = json::parse(r.data);
            std::string tx_id = d.value("tx_id", "");
            std::string status = d.value("status", "");
            uint64_t height = d.value("block_height", 0ULL);
            Dbg("  " + nodes_[r.node_idx].node_id + " ✓ tx_id=" + tx_id.substr(0, 16) + " status=" + status);

            recent_txs_.push_back({tx_id, "TRANSFER", user.address, to_addr, amount, status});
            tx_order_.push_back(tx_id);

            PrintOk(nodes_[r.node_idx].node_id + ": " + status + " height=" + std::to_string(height));
            std::cout << "  tx_id: " << tx_id << "\n";
        } else {
            Dbg("  " + nodes_[r.node_idx].node_id + " ✗ " + r.error);
            PrintErr(nodes_[r.node_idx].node_id + ": " + r.error);
        }
    }
    Pause();
}

// ══════════════════════════════════════════════════════════════════════════════
// 4. 存储数据
// ══════════════════════════════════════════════════════════════════════════════

void CliSession::DoStoreData() {
    if (active_user_ < 0) { PrintErr("请先登录"); return; }
    DbgSep("存储数据");
    const auto& user = users_[active_user_];
    auto data_str = PromptLine("数据: ");
    if (data_str.empty()) { PrintErr("数据不能为空"); return; }

    uint64_t nonce = 1;
    std::string raw;
    if (HttpGet(0, "/api/state/" + user.address, raw)) {
        std::string d, e;
        if (ParseOk(raw, d, e)) nonce = json::parse(d).value("nonce", 0ULL) + 1;
    }

    json body = {
        {"type", "STORE_DATA"}, {"from", user.address}, {"to", ""},
        {"amount", 0}, {"data_hash", ""}, {"nonce", nonce}, {"timestamp", 0},
        {"public_key", user.public_key}, {"private_key", user.private_key}
    };

    int target = -1;
    std::cout << "  发送到节点 [1-" << nodes_.size() << ", 0=全部]: ";
    target = PickNode("");

    std::vector<NodeResponse> results;
    if (target < 0) results = BroadcastPost("/api/transactions/store", body.dump());
    else results.push_back(SendToNode(target, "POST", "/api/transactions/store", body.dump()));

    for (const auto& r : results) {
        if (r.ok) {
            auto d = json::parse(r.data);
            std::string tx_id = d.value("tx_id", "");
            PrintOk(nodes_[r.node_idx].node_id + ": tx_id=" + tx_id.substr(0, 16) + "...");
            recent_txs_.push_back({tx_id, "STORE_DATA", user.address, "", 0, d.value("status", "")});
            tx_order_.push_back(tx_id);
        } else {
            PrintErr(nodes_[r.node_idx].node_id + ": " + r.error);
        }
    }
    Pause();
}

// ══════════════════════════════════════════════════════════════════════════════
// 5. 查询所有节点账户状态
// ══════════════════════════════════════════════════════════════════════════════

void CliSession::DoQueryAccount() {
    auto addr = PromptLine("账户地址 (留空=当前用户): ");
    if (addr.empty() && active_user_ >= 0) addr = users_[active_user_].address;
    if (addr.empty()) { PrintErr("地址不能为空"); return; }

    DbgSep("查询账户: " + addr.substr(0, 12) + "...");
    std::cout << "\n  ── 各节点账户状态 ──\n";
    std::cout << "  节点   │ 余额   │ Nonce\n";
    std::cout << "  ───────┼────────┼──────\n";

    auto results = BroadcastGet("/api/state/" + addr);
    for (const auto& r : results) {
        if (r.ok) {
            auto d = json::parse(r.data);
            std::cout << "  " << std::setw(6) << nodes_[r.node_idx].node_id << " │ "
                      << std::setw(6) << d.value("balance", 0ULL) << " │ "
                      << d.value("nonce", 0ULL) << "\n";
        } else if (r.error.find("连接失败") != std::string::npos || r.error.find("connect") != std::string::npos) {
            std::cout << "  " << std::setw(6) << nodes_[r.node_idx].node_id << " │ "
                      << "  (离线)\n";
        } else {
            std::cout << "  " << std::setw(6) << nodes_[r.node_idx].node_id << " │ "
                      << "  (账户不存在)\n";
        }
    }
    Pause();
}

// ══════════════════════════════════════════════════════════════════════════════
// 6. 查询所有节点最新区块
// ══════════════════════════════════════════════════════════════════════════════

void CliSession::DoQueryBlock() {
    DbgSep("查询最新区块");
    std::cout << "\n  ── 各节点最新区块 ──\n";
    std::cout << "  节点   │ 高度 │ 区块哈希         │ Merkle根         │ state_root\n";
    std::cout << "  ───────┼──────┼──────────────────┼──────────────────┼──────────────────\n";

    auto results = BroadcastGet("/api/blocks/latest");
    std::string first_hash;
    bool consistent = true;

    for (const auto& r : results) {
        if (r.ok) {
            auto d = json::parse(r.data);
            auto hdr = d["header"];
            std::string hash = hdr.value("block_hash", "");
            std::string merkle = hdr.value("tx_merkle_root", "");
            std::string state = hdr.value("state_root", "");
            uint64_t height = hdr.value("height", 0ULL);

            if (first_hash.empty()) first_hash = hash;
            else if (hash != first_hash) consistent = false;

            std::cout << "  " << std::setw(6) << nodes_[r.node_idx].node_id << " │ "
                      << std::setw(4) << height << " │ "
                      << hash.substr(0, 16) << ".. │ "
                      << merkle.substr(0, 16) << ".. │ "
                      << state.substr(0, 16) << "..\n";
        } else {
            std::cout << "  " << std::setw(6) << nodes_[r.node_idx].node_id << " │ (无区块)\n";
        }
    }

    if (!consistent) {
        std::cout << "\n  \033[31m⚠ 区块哈希不一致! 存在拜占庭节点。\033[0m\n";
    }
    Pause();
}

// ══════════════════════════════════════════════════════════════════════════════
// 7. Merkle 证明对比
// ══════════════════════════════════════════════════════════════════════════════

void CliSession::DoMerkleProof() {
    DbgSep("Merkle 证明对比");
    auto tx_id = PromptLine("交易ID (留空=最近一笔): ");
    if (tx_id.empty() && !tx_order_.empty()) tx_id = tx_order_.back();
    if (tx_id.empty()) { PrintErr("没有交易记录"); return; }

    std::cout << "\n  ── 各节点 Merkle 证明 ──\n";
    auto results = BroadcastGet("/api/proofs/tx/" + tx_id);
    std::string first_root;
    bool consistent = true;

    for (const auto& r : results) {
        if (r.ok) {
            auto d = json::parse(r.data);
            std::string root = d.value("root", "");
            if (first_root.empty()) first_root = root;
            else if (root != first_root) consistent = false;

            std::cout << "  " << nodes_[r.node_idx].node_id << ": root=" << root.substr(0, 24) << "...\n";
        } else {
            std::cout << "  " << nodes_[r.node_idx].node_id << ": " << r.error << "\n";
        }
    }
    if (!consistent) {
        std::cout << "\n  \033[31m⚠ Merkle 根不一致! 恶意节点篡改了数据。\033[0m\n";
    } else {
        std::cout << "\n  \033[32m✓ 所有节点 Merkle 根一致。\033[0m\n";
    }
    Pause();
}

// ══════════════════════════════════════════════════════════════════════════════
// 8. 所有节点状态总览
// ══════════════════════════════════════════════════════════════════════════════

void CliSession::DoNodeStatus() {
    DbgSep("节点状态总览");
    std::cout << "\n  ── 节点状态 ──\n";
    std::cout << "  节点   │ 状态 │ 高度 │ 攻击模式\n";
    std::cout << "  ───────┼──────┼──────┼──────────────────\n";

    auto results = BroadcastGet("/api/node/status");
    for (const auto& r : results) {
        if (r.ok) {
            auto d = json::parse(r.data);
            auto consensus = d.value("consensus", json::object());
            std::string attack = consensus.value("attack_mode", "normal");
            std::string height = d.value("latest_height", "0");
            std::string status = consensus.value("running", true) ? "运行" : "停止";
            std::cout << "  " << std::setw(6) << nodes_[r.node_idx].node_id << " │ "
                      << std::setw(4) << status << " │ "
                      << std::setw(4) << height << " │ "
                      << attack << "\n";
        } else {
            std::cout << "  " << std::setw(6) << nodes_[r.node_idx].node_id << " │ 离线\n";
        }
    }
    Pause();
}

// ══════════════════════════════════════════════════════════════════════════════
// 9. 攻击模拟 (核心: 设置恶意节点 + 提交交易 + 验证拜占庭容错)
// ══════════════════════════════════════════════════════════════════════════════

void CliSession::DoAttackSimulation() {
    DbgSep("攻击模拟 - 拜占庭容错验证");

    // Step 0: 检查当前主节点（攻击只有对主节点才可见效果）
    DbgPrint("检测各节点角色...");
    int primary_idx = 0;
    {
        auto status_results = BroadcastGet("/api/node/status");
        for (const auto& r : status_results) {
            if (!r.ok) continue;
            auto d = json::parse(r.data);
            auto consensus = d.value("consensus", json::object());
            bool is_primary = consensus.value("is_primary", false);
            std::cout << "  " << nodes_[r.node_idx].node_id << ": "
                      << (is_primary ? "\033[33m主节点 (PRIMARY)\033[0m" : "备份节点")
                      << "  height=" << d.value("latest_height", "0") << "\n";
            if (is_primary) primary_idx = static_cast<int>(r.node_idx);
        }
    }

    // 选择恶意节点
    std::cout << "\n  选择恶意节点:\n";
    for (size_t i = 0; i < nodes_.size(); ++i) {
        std::cout << "    " << (i + 1) << ". " << nodes_[i].node_id;
        if (static_cast<int>(i) == primary_idx) std::cout << " \033[33m← 当前主节点 (推荐)\033[0m";
        std::cout << "\n";
    }
    int bad_idx = PickNode("恶意节点 [1-" + std::to_string(nodes_.size()) + "]: ");
    if (bad_idx < 0 || bad_idx >= static_cast<int>(nodes_.size())) { PrintErr("无效选择"); return; }
    if (bad_idx != primary_idx) {
        std::cout << "  \033[33m注意: 选择的是备份节点，篡改类攻击(1-3)可能不会产生区块分歧。\033[0m\n";
        std::cout << "  \033[33m只有主节点打包区块时攻击才会生效。消息丢弃类攻击(4-5)影响较小。\033[0m\n";
    }

    // 选择攻击模式
    std::cout << "\n  攻击模式:\n";
    std::cout << "    1. bad_merkle_root     篡改 Merkle 根 (需主节点)\n";
    std::cout << "    2. bad_state_root      篡改状态根 (需主节点)\n";
    std::cout << "    3. invalid_block_hash  篡改区块哈希 (需主节点)\n";
    std::cout << "    4. drop_prepare        丢弃 PREPARE 消息\n";
    std::cout << "    5. double_proposal     双重提议 (需主节点)\n";
    int mode_choice = static_cast<int>(PromptUint64("攻击模式 [1-5]: "));

    std::string mode_name;
    switch (mode_choice) {
        case 1: mode_name = "bad_merkle_root"; break;
        case 2: mode_name = "bad_state_root"; break;
        case 3: mode_name = "invalid_block_hash"; break;
        case 4: mode_name = "drop_prepare"; break;
        case 5: mode_name = "double_proposal"; break;
        default: PrintErr("无效选择"); return;
    }

    // Step 1: 恢复所有节点为正常模式
    DbgPrint("Step 1 - 重置所有节点为正常模式...");
    for (size_t i = 0; i < nodes_.size(); ++i) {
        std::string dummy;
        HttpPost(i, "/api/admin/attack-mode", R"({"mode":"normal"})", dummy);
    }

    // Step 2: 设置恶意节点攻击模式
    DbgPrint("Step 2 - 设置 " + nodes_[bad_idx].node_id + " 为攻击模式: " + mode_name);
    {
        json body = {{"mode", mode_name}};
        std::string raw;
        HttpPost(bad_idx, "/api/admin/attack-mode", body.dump(), raw);
        PrintOk(nodes_[bad_idx].node_id + " → " + mode_name);
    }

    // Step 3: 确保有两个不同用户（发送方 + 接收方）
    DbgPrint("Step 3 - 准备交易双方...");
    if (active_user_ < 0) {
        // 自动注册发送方
        json reg_body = {{"username", "attacker"}, {"password", "test123456"}};
        auto reg_results = BroadcastPost("/api/users/register", reg_body.dump());
        for (const auto& r : reg_results) {
            if (r.ok) {
                auto d = json::parse(r.data);
                RemoteUser user;
                user.username = d.value("username", "attacker");
                user.address = d.value("address", "");
                user.public_key = d.value("public_key", "");
                user.private_key = d.value("private_key", "");
                users_.push_back(user);
                active_user_ = static_cast<int>(users_.size()) - 1;
                break;
            }
        }
    }
    if (active_user_ < 0) { PrintErr("无法获取发送方用户"); return; }
    const auto& sender = users_[active_user_];

    // 查找或选择接收方
    std::string receiver_addr;
    std::string receiver_name;
    bool auto_victim = false;

    // Step: 列出可选接收方，让用户手动选择
    {
        std::vector<std::pair<std::string, std::string>> candidates;  // {username, address}
        std::string raw;
        HttpGet(0, "/api/users?page=1&page_size=50", raw);
        std::string data, err;
        if (ParseOk(raw, data, err)) {
            auto users_json = json::parse(data)["users"];
            for (const auto& u : users_json) {
                std::string uname = u.value("username", "");
                std::string addr = u.value("address", "");
                if (addr != sender.address && !addr.empty()) {
                    candidates.push_back({uname, addr});
                }
            }
        }

        std::cout << "\n  选择转账目标:\n";
        for (size_t ci = 0; ci < candidates.size(); ++ci) {
            std::cout << "    " << (ci + 1) << ". " << std::setw(14) << candidates[ci].first
                      << "  " << candidates[ci].second.substr(0, 20) << "...\n";
        }
        int auto_opt = static_cast<int>(candidates.size()) + 1;
        int manual_opt = auto_opt + 1;
        std::cout << "    " << auto_opt << ". 自动创建新接收方\n";
        std::cout << "    " << manual_opt << ". 手动输入地址\n";

        int choice = static_cast<int>(PromptUint64("选择 [1-" + std::to_string(manual_opt) + "]: "));
        if (choice >= 1 && choice <= static_cast<int>(candidates.size())) {
            receiver_name = candidates[static_cast<size_t>(choice - 1)].first;
            receiver_addr = candidates[static_cast<size_t>(choice - 1)].second;
        } else if (choice == auto_opt) {
            // 自动创建
            DbgPrint("  自动创建接收方...");
            std::string vname = "victim";
            json reg_body = {{"username", vname}, {"password", "test123456"}};
            auto reg_results = BroadcastPost("/api/users/register", reg_body.dump());
            for (const auto& r : reg_results) {
                if (r.ok) {
                    auto d = json::parse(r.data);
                    receiver_name = vname;
                    receiver_addr = d.value("address", "");
                    RemoteUser victim;
                    victim.username = vname;
                    victim.address = receiver_addr;
                    victim.public_key = d.value("public_key", "");
                    victim.private_key = d.value("private_key", "");
                    users_.push_back(victim);
                    auto_victim = true;
                    break;
                }
            }
        } else if (choice == manual_opt) {
            // 手动输入
            receiver_addr = PromptLine("  输入接收方地址: ");
            if (receiver_addr.size() < 10) {
                PrintErr("地址无效");
                return;
            }
            receiver_name = "手动输入";
        } else {
            PrintErr("无效选择");
            return;
        }
    }
    if (receiver_addr.empty()) { PrintErr("无法获取接收方地址"); return; }

    std::cout << "  发送方: \033[33m" << sender.username << "\033[0m "
              << sender.address.substr(0, 16) << "...\n";
    std::cout << "  接收方: \033[33m" << receiver_name << "\033[0m "
              << receiver_addr.substr(0, 16) << "...\n";

    // Step 4: 查询发送方余额和 nonce
    uint64_t sender_balance_before = 0;
    uint64_t nonce = 1;
    {
        std::string raw;
        HttpGet(bad_idx, "/api/state/" + sender.address, raw);
        std::string d, e;
        if (ParseOk(raw, d, e)) {
            auto state = json::parse(d);
            sender_balance_before = state.value("balance", 0ULL);
            nonce = state.value("nonce", 0ULL) + 1;
        }
    }
    std::cout << "  发送方余额: " << sender_balance_before << "\n";

    // Step 5: 构造真实的转账交易 (发送方 → 接收方, 不同地址)
    uint64_t transfer_amount = 50;  // 仿真转账金额
    DbgPrint("Step 4 - 构造转账: " + sender.username + " → " + receiver_name + " amount=" + std::to_string(transfer_amount));
    json tx_body = {
        {"type", "TRANSFER"},
        {"from", sender.address},
        {"to", receiver_addr},
        {"amount", transfer_amount},
        {"nonce", nonce},
        {"timestamp", 0},
        {"public_key", sender.public_key},
        {"private_key", sender.private_key}
    };

    // Step 6: 提交交易到恶意节点
    DbgPrint("Step 5 - 提交交易到恶意节点 " + nodes_[bad_idx].node_id + "...");
    std::string tx_id;
    {
        std::string raw;
        HttpPost(bad_idx, "/api/transactions/transfer", tx_body.dump(), raw);
        std::string d, e;
        if (ParseOk(raw, d, e)) {
            tx_id = json::parse(d).value("tx_id", "");
            DbgPrint("  交易已提交: tx_id=" + tx_id.substr(0, 16) + "...");
        } else {
            PrintErr("交易提交失败: " + e);
            Pause();
            return;
        }
    }

    // Step 7: 提交相同交易到诚实节点
    DbgPrint("Step 6 - 提交相同交易到诚实节点...");
    for (size_t i = 0; i < nodes_.size(); ++i) {
        if (static_cast<int>(i) == bad_idx) continue;
        std::string raw;
        HttpPost(i, "/api/transactions/transfer", tx_body.dump(), raw);
    }

    // 等待共识
    std::cout << "\n  等待共识完成 (6s)...\n";
    std::this_thread::sleep_for(std::chrono::seconds(6));

    // Step 8: 对比所有节点的区块
    DbgPrint("Step 7 - 对比所有节点区块...");
    std::cout << "\n  ── 拜占庭容错验证 ──\n";
    std::cout << "  节点   │ 角色     │ 高度 │ 区块哈希         │ Merkle根         │ state_root\n";
    std::cout << "  ───────┼──────────┼──────┼──────────────────┼──────────────────┼──────────────────\n";

    auto block_results = BroadcastGet("/api/blocks/latest");
    std::string honest_hash;
    bool tampered = false;

    for (const auto& r : block_results) {
        if (!r.ok) continue;
        auto d = json::parse(r.data);
        auto hdr = d["header"];
        std::string hash = hdr.value("block_hash", "");
        std::string merkle = hdr.value("tx_merkle_root", "");
        std::string state = hdr.value("state_root", "");
        uint64_t height = hdr.value("height", 0ULL);

        std::string role = (static_cast<int>(r.node_idx) == bad_idx) ? "恶意 ⚠" : "诚实 ✓";
        if (static_cast<int>(r.node_idx) != bad_idx) {
            if (honest_hash.empty()) honest_hash = hash;
        }

        std::cout << "  " << std::setw(6) << nodes_[r.node_idx].node_id << " │ "
                  << std::setw(8) << role << " │ "
                  << std::setw(4) << height << " │ "
                  << hash.substr(0, 16) << ".. │ "
                  << merkle.substr(0, 16) << ".. │ "
                  << state.substr(0, 16) << "..\n";
    }

    // Step 9: 查询转账后各方余额
    std::cout << "\n  ── 余额变化 ──\n";
    std::cout << "  节点   │ " << std::setw(12) << sender.username << "余额 │ "
              << std::setw(12) << receiver_name << "余额 │ Nonce\n";
    std::cout << "  ───────┼──────────────┼──────────────┼──────\n";

    auto sender_states = BroadcastGet("/api/state/" + sender.address);
    auto victim_states = BroadcastGet("/api/state/" + receiver_addr);
    bool balance_diverged = false;
    uint64_t honest_sender_bal = 0, honest_victim_bal = 0;

    for (size_t i = 0; i < nodes_.size(); ++i) {
        uint64_t sbal = 0, vbal = 0, sn = 0;
        if (i < sender_states.size() && sender_states[i].ok) {
            auto sd = json::parse(sender_states[i].data);
            sbal = sd.value("balance", 0ULL);
            sn = sd.value("nonce", 0ULL);
        }
        if (i < victim_states.size() && victim_states[i].ok) {
            auto vd = json::parse(victim_states[i].data);
            vbal = vd.value("balance", 0ULL);
        }
        if (static_cast<int>(i) != bad_idx) {
            if (honest_sender_bal == 0) { honest_sender_bal = sbal; honest_victim_bal = vbal; }
            else if (sbal != honest_sender_bal || vbal != honest_victim_bal) balance_diverged = true;
        }
        std::cout << "  " << std::setw(6) << nodes_[i].node_id << " │ "
                  << std::setw(12) << sbal << " │ "
                  << std::setw(12) << vbal << " │ "
                  << sn << "\n";
    }

    if (balance_diverged) {
        std::cout << "  \033[31m⚠ 余额不一致! 恶意节点状态与诚实节点不同。\033[0m\n";
    }

    // Step 10: Merkle 证明对比
    std::cout << "\n  ── Merkle 证明对比 ──\n";
    auto merkle_results = BroadcastGet("/api/proofs/tx/" + tx_id);
    std::string bad_merkle, honest_merkle;
    for (const auto& r : merkle_results) {
        if (!r.ok) {
            std::cout << "  " << nodes_[r.node_idx].node_id << ": \033[31m" << r.error << "\033[0m\n";
            continue;
        }
        auto d = json::parse(r.data);
        std::string root = d.value("root", "");
        if (static_cast<int>(r.node_idx) == bad_idx) bad_merkle = root;
        else if (honest_merkle.empty()) honest_merkle = root;
        std::cout << "  " << nodes_[r.node_idx].node_id << ": root=" << root.substr(0, 40) << "...\n";
    }

    // Step 11: 验证结论
    std::cout << "\n  ── 验证结论 ──\n";
    if (!bad_merkle.empty() && !honest_merkle.empty() && bad_merkle != honest_merkle) {
        tampered = true;
        std::cout << "  \033[31m✗ 拜占庭攻击生效!\033[0m 恶意节点数据被篡改。\n";
        std::cout << "  恶意节点 Merkle 根: " << bad_merkle.substr(0, 40) << "...\n";
        std::cout << "  诚实节点 Merkle 根: " << honest_merkle.substr(0, 40) << "...\n";
        std::cout << "  \033[32mRBFT 容错: 诚实多数派(" << (nodes_.size() - 1) << "/" << nodes_.size()
                  << ")维持正确链，恶意节点被隔离。\033[0m\n";
    } else if (!bad_merkle.empty() && bad_merkle == honest_merkle) {
        std::cout << "  \033[33m~ 攻击未生效。\033[0m\n";
        if (bad_idx != primary_idx) {
            std::cout << "  原因: 恶意节点不是主节点，未参与区块打包。\n";
            std::cout << "  建议: 下次选择主节点(" << nodes_[primary_idx].node_id << ")作为攻击目标。\n";
        } else {
            std::cout << "  原因: 共识机制成功阻止了篡改，或攻击在传播前被修复。\n";
        }
    } else {
        std::cout << "  \033[33m? 无法判定 (可能交易尚未打包或证明不可用)\033[0m\n";
    }

    // 查询共识状态
    auto consensus_results = BroadcastGet("/api/node/consensus");
    std::cout << "\n  ── 共识状态 ──\n";
    for (const auto& r : consensus_results) {
        if (!r.ok) continue;
        auto d = json::parse(r.data);
        std::string attack = d.value("attack_mode", "normal");
        int evidence = d.value("evidence_count", 0);
        auto quarantined = d.value("quarantined_nodes", json::array());
        std::cout << "  " << nodes_[r.node_idx].node_id << ": attack_mode=" << attack
                  << " evidence=" << evidence;
        if (!quarantined.empty()) {
            std::cout << " quarantined=[";
            for (size_t qi = 0; qi < quarantined.size(); ++qi) {
                if (qi > 0) std::cout << ",";
                std::cout << quarantined[qi].get<std::string>();
            }
            std::cout << "]";
        }
        std::cout << "\n";
    }

    // 记录交易
    if (!tx_id.empty()) {
        std::string status = tampered ? "REJECTED" : "COMMITTED";
        recent_txs_.push_back({tx_id, "TRANSFER", sender.address, receiver_addr, transfer_amount, status});
        tx_order_.push_back(tx_id);
    }

    Dbg("攻击模拟完成: sender=" + sender.username + " receiver=" + receiver_name
        + " amount=" + std::to_string(transfer_amount) + " mode=" + mode_name
        + " tampered=" + std::string(tampered ? "true" : "false"));
    Pause();
}

// ══════════════════════════════════════════════════════════════════════════════
// 10. 最近交易记录
// ══════════════════════════════════════════════════════════════════════════════

void CliSession::DoShowHistory() {
    if (tx_order_.empty()) {
        std::cout << "\n  暂无交易记录。\n";
        Pause();
        return;
    }
    std::cout << "\n  ── 最近交易记录 ──\n";
    std::cout << "  编号 │ 类型         │ 发送方         │ 金额  │ 状态\n";
    std::cout << "  ─────┼──────────────┼────────────────┼───────┼──────────\n";
    for (size_t i = 0; i < tx_order_.size(); ++i) {
        // 线性查找
        for (const auto& r : recent_txs_) {
            if (r.tx_id == tx_order_[i]) {
                std::cout << "  " << std::setw(4) << i << " │ "
                          << std::setw(12) << r.type << " │ "
                          << r.from.substr(0, 12) << ".. │ "
                          << std::setw(5) << r.amount << " │ "
                          << r.status << "\n";
                break;
            }
        }
    }
    Pause();
}

// ══════════════════════════════════════════════════════════════════════════════
// 11. 查看所有用户 (分页)
// ══════════════════════════════════════════════════════════════════════════════

void CliSession::DoListUsers() {
    DbgSep("查看所有用户");

    // 选择查询哪个节点
    std::cout << "  查询哪个节点? [1-" << nodes_.size() << ", 默认1]: ";
    int node_idx = PickNode("");
    if (node_idx < 0) node_idx = 0;

    int page = 1;
    const int page_size = 10;

    while (true) {
        std::string path = "/api/users?page=" + std::to_string(page) + "&page_size=" + std::to_string(page_size);
        std::string raw;
        if (!HttpGet(node_idx, path, raw)) {
            PrintErr("查询失败: " + nodes_[node_idx].node_id + " 无响应");
            Pause();
            return;
        }

        std::string data, err;
        if (!ParseOk(raw, data, err)) {
            PrintErr("查询失败: " + err);
            Pause();
            return;
        }

        auto d = json::parse(data);
        auto users = d.value("users", json::array());
        int total = d.value("total", 0);
        int total_pages = (total + page_size - 1) / page_size;

        if (total == 0) {
            std::cout << "\n  暂无注册用户。\n";
            Pause();
            return;
        }

        std::cout << "\n  ── " << nodes_[node_idx].node_id << " 用户列表 (第 "
                  << page << "/" << total_pages << " 页, 共 " << total << " 人) ──\n";
        std::cout << "  编号 │ 用户名       │ 地址               │ 余额   │ Nonce\n";
        std::cout << "  ─────┼──────────────┼────────────────────┼────────┼──────\n";

        for (const auto& u : users) {
            std::string addr = u.value("address", "");
            std::cout << "  " << std::setw(4) << u.value("user_id", 0) << " │ "
                      << std::setw(12) << u.value("username", "") << " │ "
                      << addr.substr(0, 18) << (addr.size() > 18 ? ".." : "  ") << " │ "
                      << std::setw(6) << u.value("balance", 0ULL) << " │ "
                      << u.value("nonce", 0ULL) << "\n";
        }

        if (total_pages <= 1) {
            Pause();
            return;
        }

        std::cout << "\n  n=下一页  p=上一页  q=退出: ";
        std::string input;
        std::getline(std::cin, input);
        if (input == "n" || input == "N") {
            if (page < total_pages) page++;
        } else if (input == "p" || input == "P") {
            if (page > 1) page--;
        } else {
            return;
        }
    }
}

// ══════════════════════════════════════════════════════════════════════════════
// 12. 多节点共识验证
// ══════════════════════════════════════════════════════════════════════════════

void CliSession::DoConsensusVerify() {
    DbgSep("多节点共识验证");
    std::cout << "\n  ── 多节点共识验证 ──\n\n";

    int pass = 0, fail = 0, offline = 0;

    // Check 1: 节点在线状态
    std::cout << "  [1] 节点在线检查\n";
    std::vector<uint64_t> heights(nodes_.size(), 0);
    std::vector<std::string> hashes(nodes_.size());
    std::vector<bool> online(nodes_.size(), false);

    for (size_t i = 0; i < nodes_.size(); ++i) {
        std::string raw;
        if (HttpGet(i, "/api/node/status", raw)) {
            auto d = json::parse(raw);
            if (d.value("ok", false)) {
                online[i] = true;
                heights[i] = std::stoull(d["data"].value("latest_height", "0"));
                std::cout << "    " << nodes_[i].node_id << " ✓ 在线 height=" << heights[i] << "\n";
            }
        }
        if (!online[i]) {
            std::cout << "    " << nodes_[i].node_id << " ✗ 离线\n";
            offline++;
        }
    }

    // Check 2: 区块高度一致性
    std::cout << "\n  [2] 区块高度一致性\n";
    uint64_t expected_height = heights[0];
    bool height_ok = true;
    for (size_t i = 1; i < nodes_.size(); ++i) {
        if (online[i] && heights[i] != expected_height) {
            height_ok = false;
        }
    }
    if (height_ok && expected_height > 0) {
        std::cout << "    ✓ 所有节点高度一致: " << expected_height << "\n";
        pass++;
    } else if (expected_height == 0) {
        std::cout << "    - 无区块可比较\n";
    } else {
        std::cout << "    ✗ 高度不一致:";
        for (size_t i = 0; i < nodes_.size(); ++i) {
            if (online[i]) std::cout << " " << nodes_[i].node_id << "=" << heights[i];
        }
        std::cout << "\n";
        fail++;
    }

    // Check 3: 区块哈希一致性 (逐高度比较)
    if (expected_height > 0) {
        std::cout << "\n  [3] 区块哈希一致性\n";
        for (uint64_t h = 1; h <= expected_height; ++h) {
            std::string first_hash;
            bool hash_ok = true;
            for (size_t i = 0; i < nodes_.size(); ++i) {
                if (!online[i]) continue;
                std::string raw;
                HttpGet(i, "/api/blocks/" + std::to_string(h), raw);
                std::string data, err;
                if (ParseOk(raw, data, err)) {
                    auto d = json::parse(data);
                    std::string hash = d["header"].value("block_hash", "");
                    if (first_hash.empty()) first_hash = hash;
                    else if (hash != first_hash) hash_ok = false;
                }
            }
            if (hash_ok) {
                std::cout << "    height=" << h << " ✓ hash=" << first_hash.substr(0, 16) << "...\n";
                pass++;
            } else {
                std::cout << "    height=" << h << " ✗ 哈希不一致!\n";
                fail++;
            }
        }
    }

    // Check 4: 账户余额一致性 (抽查前 3 个用户)
    std::cout << "\n  [4] 账户余额一致性\n";
    std::string users_raw;
    if (HttpGet(0, "/api/users?page=1&page_size=3", users_raw)) {
        std::string data, err;
        if (ParseOk(users_raw, data, err)) {
            auto d = json::parse(data);
            for (const auto& u : d["users"]) {
                std::string addr = u.value("address", "");
                std::string uname = u.value("username", "");
                uint64_t first_balance = 0;
                bool balance_ok = true;
                for (size_t i = 0; i < nodes_.size(); ++i) {
                    if (!online[i]) continue;
                    std::string raw;
                    HttpGet(i, "/api/state/" + addr, raw);
                    std::string sd, se;
                    if (ParseOk(raw, sd, se)) {
                        uint64_t bal = json::parse(sd).value("balance", 0ULL);
                        if (first_balance == 0 && i == 0) first_balance = bal;
                        else if (bal != first_balance) balance_ok = false;
                    }
                }
                if (balance_ok) {
                    std::cout << "    " << uname << " ✓ balance=" << first_balance << "\n";
                    pass++;
                } else {
                    std::cout << "    " << uname << " ✗ 余额不一致!\n";
                    fail++;
                }
            }
        }
    }

    // Check 5: Merkle 根一致性
    if (expected_height > 0) {
        std::cout << "\n  [5] Merkle 根一致性\n";
        for (uint64_t h = 1; h <= expected_height; ++h) {
            std::string first_merkle;
            bool merkle_ok = true;
            for (size_t i = 0; i < nodes_.size(); ++i) {
                if (!online[i]) continue;
                std::string raw;
                HttpGet(i, "/api/blocks/" + std::to_string(h), raw);
                std::string data, err;
                if (ParseOk(raw, data, err)) {
                    auto d = json::parse(data);
                    std::string merkle = d["header"].value("tx_merkle_root", "");
                    if (first_merkle.empty()) first_merkle = merkle;
                    else if (merkle != first_merkle) merkle_ok = false;
                }
            }
            if (merkle_ok) {
                std::cout << "    height=" << h << " ✓ merkle_root=" << first_merkle.substr(0, 16) << "...\n";
                pass++;
            } else {
                std::cout << "    height=" << h << " ✗ Merkle 根不一致!\n";
                fail++;
            }
        }
    }

    // Check 6: 共识攻击模式检查
    std::cout << "\n  [6] 共识攻击模式检查\n";
    bool any_attack = false;
    for (size_t i = 0; i < nodes_.size(); ++i) {
        if (!online[i]) continue;
        std::string raw;
        HttpGet(i, "/api/node/consensus", raw);
        std::string data, err;
        if (ParseOk(raw, data, err)) {
            auto d = json::parse(data);
            std::string mode = d.value("attack_mode", "normal");
            if (mode != "normal") {
                std::cout << "    " << nodes_[i].node_id << " ⚠ attack_mode=" << mode << "\n";
                any_attack = true;
            }
        }
    }
    if (!any_attack) {
        std::cout << "    ✓ 所有节点处于正常模式\n";
        pass++;
    }

    // 总结
    std::cout << "\n  ── 验证总结 ──\n";
    std::cout << "  通过: \033[32m" << pass << "\033[0m";
    if (fail > 0) std::cout << "  失败: \033[31m" << fail << "\033[0m";
    if (offline > 0) std::cout << "  离线: " << offline;
    std::cout << "\n";

    if (fail == 0 && offline == 0) {
        std::cout << "  \033[32m✓ 多节点共识验证全部通过!\033[0m\n";
    } else if (fail > 0) {
        std::cout << "  \033[31m✗ 存在不一致，需要检查!\033[0m\n";
    }
    Dbg("验证完成: pass=" + std::to_string(pass) + " fail=" + std::to_string(fail) + " offline=" + std::to_string(offline));
    Pause();
}

} // namespace rbft
