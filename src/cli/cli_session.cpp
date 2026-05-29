#include "cli/cli_session.h"

#include "common/logger.h"
#include "crypto/crypto_utils.h"
#include "user/account_state.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace rbft {

// ══════════════════════════════════════════════════════════════════════════════
// 构造 / 析构
// ══════════════════════════════════════════════════════════════════════════════

CliSession::CliSession(const NodeConfig& config)
    : config_(config),
      users_(std::make_unique<UserManager>(&storage_)),
      consensus_(config),
      executor_(&storage_) {
    // 确保数据目录存在
    std::filesystem::create_directories(std::filesystem::path(config_.db_path).parent_path());
    storage_.Open(config_.db_path);
    storage_.InitializeSchema();
    OpenDebugLog();
    DbgSep("CLI 会话启动");
    DbgPrint("节点ID: " + config_.node_id);
    DbgPrint("数据库: " + config_.db_path);
    DbgPrint("链ID:   " + config_.chain_id);
    Dbg("f=" + std::to_string(config_.f) +
        " instance_count=" + std::to_string(config_.instance_count) +
        " quorum=" + std::to_string(consensus_.Quorum()));
}

CliSession::~CliSession() {
    DbgSep("CLI 会话结束");
    if (debug_log_.is_open()) debug_log_.close();
}

// ══════════════════════════════════════════════════════════════════════════════
// 调试日志
// ══════════════════════════════════════════════════════════════════════════════

void CliSession::OpenDebugLog() {
    std::string log_path = "data/" + config_.node_id + "/cli_debug.log";
    debug_log_.open(log_path, std::ios::app);
    if (!debug_log_.is_open()) {
        std::cerr << "[WARN] 无法打开调试日志: " << log_path << "\n";
    }
}

void CliSession::Dbg(const std::string& msg) {
    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    char ts[32]{};
    std::strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    std::string line = std::string(ts) + " [DEBUG] " + msg;
    if (debug_log_.is_open()) {
        debug_log_ << line << "\n";
        debug_log_.flush();
    }
}

void CliSession::DbgPrint(const std::string& msg) {
    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    char ts[32]{};
    std::strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    std::string line = std::string(ts) + " [DEBUG] " + msg;
    if (debug_log_.is_open()) {
        debug_log_ << line << "\n";
        debug_log_.flush();
    }
    std::cerr << "\033[90m" << line << "\033[0m\n";
}

void CliSession::DbgSep(const std::string& title) {
    std::string sep(60, '=');
    Dbg(sep);
    Dbg("  " + title);
    Dbg(sep);
}

// ══════════════════════════════════════════════════════════════════════════════
// 辅助函数
// ══════════════════════════════════════════════════════════════════════════════

std::string CliSession::PromptLine(const std::string& prompt) const {
    std::cout << prompt;
    std::cout.flush();
    std::string line;
    if (!std::getline(std::cin, line)) return "";
    return line;
}

uint64_t CliSession::PromptUint64(const std::string& prompt) const {
    auto s = PromptLine(prompt);
    try {
        return std::stoull(s);
    } catch (...) {
        return 0;
    }
}

void CliSession::PrintOk(const std::string& msg) const {
    std::cout << "\033[32m[OK]\033[0m " << msg << "\n";
}

void CliSession::PrintErr(const std::string& msg) const {
    std::cout << "\033[31m[ERR]\033[0m " << msg << "\n";
}

void CliSession::Pause() const {
    std::cout << "\n按 Enter 继续...";
    std::cin.get();
}

// ══════════════════════════════════════════════════════════════════════════════
// 菜单
// ══════════════════════════════════════════════════════════════════════════════

void CliSession::ShowMenu() const {
    std::cout << "\n";
    std::cout << "\033[36m╔══════════════════════════════════════════╗\033[0m\n";
    std::cout << "\033[36m║\033[0m  \033[1mRBFT Chain Demo - 交互式命令行\033[0m          \033[36m║\033[0m\n";
    std::cout << "\033[36m╠══════════════════════════════════════════╣\033[0m\n";
    std::cout << "\033[36m║\033[0m  1. 注册用户                              \033[36m║\033[0m\n";
    std::cout << "\033[36m║\033[0m  2. 用户登录                              \033[36m║\033[0m\n";
    std::cout << "\033[36m║\033[0m  3. 转账交易                              \033[36m║\033[0m\n";
    std::cout << "\033[36m║\033[0m  4. 存储数据 (STORE_DATA)                 \033[36m║\033[0m\n";
    std::cout << "\033[36m║\033[0m  5. 查询账户状态                          \033[36m║\033[0m\n";
    std::cout << "\033[36m║\033[0m  6. 查询区块                              \033[36m║\033[0m\n";
    std::cout << "\033[36m║\033[0m  7. 查询交易                              \033[36m║\033[0m\n";
    std::cout << "\033[36m║\033[0m  8. 查询待处理交易 (mempool)              \033[36m║\033[0m\n";
    std::cout << "\033[36m║\033[0m  9. Merkle 证明验证                       \033[36m║\033[0m\n";
    std::cout << "\033[36m║\033[0m 10. SMT 状态证明验证                      \033[36m║\033[0m\n";
    std::cout << "\033[36m║\033[0m 11. 攻击模拟                              \033[36m║\033[0m\n";
    std::cout << "\033[36m║\033[0m 12. 节点状态                              \033[36m║\033[0m\n";
    std::cout << "\033[36m║\033[0m 13. 共识状态                              \033[36m║\033[0m\n";
    std::cout << "\033[36m║\033[0m 14. 最近交易记录                          \033[36m║\033[0m\n";
    std::cout << "\033[36m║\033[0m 15. 查看已注册用户 (管理)                 \033[36m║\033[0m\n";
    std::cout << "\033[36m║\033[0m  0. 退出                                  \033[36m║\033[0m\n";
    std::cout << "\033[36m╚══════════════════════════════════════════╝\033[0m\n";
    if (active_user_index_ >= 0) {
        const auto& u = logged_in_users_[active_user_index_];
        std::cout << "  当前用户: \033[33m" << u.username << "\033[0m  "
                  << "地址: " << u.address.substr(0, 12) << "...\n";
    }
    std::cout << "\n请选择 [0-15]: ";
}

void CliSession::Run() {
    while (true) {
        ShowMenu();
        std::string choice;
        if (!std::getline(std::cin, choice)) break;  // EOF 退出
        if (choice.empty()) continue;

        try {
            int c = std::stoi(choice);
            switch (c) {
                case 0:  return;
                case 1:  DoRegister(); break;
                case 2:  DoLogin(); break;
                case 3:  DoTransfer(); break;
                case 4:  DoStoreData(); break;
                case 5:  DoQueryAccount(); break;
                case 6:  DoQueryBlock(); break;
                case 7:  DoQueryTransaction(); break;
                case 8:  DoQueryPending(); break;
                case 9:  DoMerkleProof(); break;
                case 10: DoSMTProof(); break;
                case 11: DoAttackSimulation(); break;
                case 12: DoNodeStatus(); break;
                case 13: DoConsensusStatus(); break;
                case 14: DoShowHistory(); break;
                case 15: DoListUsers(); break;
                default: PrintErr("无效选择"); break;
            }
        } catch (const std::exception& e) {
            PrintErr(std::string("操作异常: ") + e.what());
            Dbg("异常: " + std::string(e.what()));
        }
    }
}

// ══════════════════════════════════════════════════════════════════════════════
// 1. 注册用户
// ══════════════════════════════════════════════════════════════════════════════

void CliSession::DoRegister() {
    DbgSep("用户注册");
    auto username = PromptLine("请输入用户名: ");
    auto password = PromptLine("请输入密码: ");
    if (username.empty() || password.empty()) {
        PrintErr("用户名和密码不能为空");
        return;
    }
    DbgPrint("注册用户: " + username);

    auto kp = crypto::GenerateEd25519KeyPair();
    Dbg("生成 Ed25519 密钥对:");
    Dbg("  private_key_hex: " + kp.private_key_hex);
    Dbg("  public_key_hex:  " + kp.public_key_hex);

    auto user = users_->Register(username, password);
    Dbg("address = SHA256(public_key)[0:40] = " + user.address);
    Dbg("密码 Argon2 哈希完成");
    Dbg("插入 users 表: user_id=" + std::to_string(user.user_id) + ", username=" + user.username);
    Dbg("创建初始账户: address=" + user.address + ", balance=1000, nonce=0");

    std::cout << "\n";
    PrintOk("用户注册成功!");
    std::cout << "  用户名:     " << user.username << "\n";
    std::cout << "  用户ID:     " << user.user_id << "\n";
    std::cout << "  地址:       " << user.address << "\n";
    std::cout << "  公钥:       " << user.public_key_hex.substr(0, 32) << "...\n";
    std::cout << "  私钥:       " << user.private_key_hex.substr(0, 32) << "... (仅演示)\n";
    std::cout << "  初始余额:   1000\n";
    Dbg("=== 注册完成 ===");
    Pause();
}

// ══════════════════════════════════════════════════════════════════════════════
// 2. 用户登录
// ══════════════════════════════════════════════════════════════════════════════

void CliSession::DoLogin() {
    DbgSep("用户登录");
    auto username = PromptLine("用户名: ");
    auto password = PromptLine("密码: ");
    DbgPrint("登录用户: " + username);

    auto login = users_->Login(username, password);
    Dbg("密码 Argon2 验证通过");
    Dbg("生成 session token: " + login.token.substr(0, 16) + "...");
    Dbg("返回密钥信息:");
    Dbg("  address:      " + login.user.address);
    Dbg("  public_key:   " + login.user.public_key_hex);
    Dbg("  private_key:  " + login.user.private_key_hex);

    CliUserSession session;
    session.username = login.user.username;
    session.address = login.user.address;
    session.public_key_hex = login.user.public_key_hex;
    session.private_key_hex = login.user.private_key_hex;
    session.token = login.token;
    logged_in_users_.push_back(session);
    active_user_index_ = static_cast<int>(logged_in_users_.size()) - 1;

    std::cout << "\n";
    PrintOk("登录成功! 当前用户: " + login.user.username);
    std::cout << "  地址: " << login.user.address << "\n";
    Dbg("=== 登录完成 ===");
    Pause();
}

// ══════════════════════════════════════════════════════════════════════════════
// 3. 转账交易
// ══════════════════════════════════════════════════════════════════════════════

void CliSession::DoTransfer() {
    if (active_user_index_ < 0) {
        PrintErr("请先登录");
        return;
    }
    DbgSep("转账交易");
    const auto& sender = logged_in_users_[active_user_index_];
    DbgPrint("发送方: " + sender.username + " (" + sender.address.substr(0, 12) + "...)");

    auto to_addr = PromptLine("接收方地址: ");
    auto amount = PromptUint64("转账金额: ");
    if (to_addr.empty() || amount == 0) {
        PrintErr("地址和金额不能为空/零");
        return;
    }

    // 获取当前 nonce
    auto account = storage_.GetAccount(sender.address);
    uint64_t nonce = account ? account->nonce + 1 : 1;
    Dbg("当前账户状态: balance=" +
        (account ? std::to_string(account->balance) : "0") +
        ", nonce=" + (account ? std::to_string(account->nonce) : "0"));
    Dbg("新 nonce: " + std::to_string(nonce));

    // 构建交易对象
    Transaction tx;
    tx.type = "TRANSFER";
    tx.from = sender.address;
    tx.to = to_addr;
    tx.amount = amount;
    tx.nonce = nonce;
    tx.timestamp = NowMillis();
    tx.public_key_hex = sender.public_key_hex;
    tx.data_hash = "";

    Dbg("构建 Transaction 对象:");
    Dbg("  type=TRANSFER");
    Dbg("  from=" + tx.from);
    Dbg("  to=" + tx.to);
    Dbg("  amount=" + std::to_string(tx.amount));
    Dbg("  nonce=" + std::to_string(tx.nonce));
    Dbg("  timestamp=" + std::to_string(tx.timestamp));

    std::string error;
    if (!SubmitAndCommit(tx, error)) {
        PrintErr("交易失败: " + error);
        return;
    }

    // 获取区块高度并保存交易记录
    uint64_t height = 0;
    auto latest = storage_.GetLatestBlock();
    if (latest) height = latest->header.height;
    TxRecord rec{tx.tx_id, tx.type, tx.from, tx.to, tx.amount, height};
    recent_tx_map_.Put(tx.tx_id, rec);
    tx_order_.push_back(tx.tx_id);

    std::cout << "\n";
    PrintOk("转账成功! tx_id=" + tx.tx_id.substr(0, 16) + "...");
    std::cout << "  tx_id:      " << tx.tx_id << "\n";
    std::cout << "  区块高度:   " << height << "\n";
    auto new_acc = storage_.GetAccount(sender.address);
    if (new_acc) {
        std::cout << "  发送方余额: " << new_acc->balance << "\n";
    }
    auto recv_acc = storage_.GetAccount(to_addr);
    if (recv_acc) {
        std::cout << "  接收方余额: " << recv_acc->balance << "\n";
    }
    Dbg("=== 转账完成 ===");
    Pause();
}

// ══════════════════════════════════════════════════════════════════════════════
// 4. 存储数据
// ══════════════════════════════════════════════════════════════════════════════

void CliSession::DoStoreData() {
    if (active_user_index_ < 0) {
        PrintErr("请先登录");
        return;
    }
    DbgSep("存储数据交易 (STORE_DATA)");
    const auto& sender = logged_in_users_[active_user_index_];
    auto data = PromptLine("请输入要存储的数据: ");
    if (data.empty()) {
        PrintErr("数据不能为空");
        return;
    }

    auto account = storage_.GetAccount(sender.address);
    uint64_t nonce = account ? account->nonce + 1 : 1;
    Dbg("当前 nonce=" + (account ? std::to_string(account->nonce) : "0") + ", 新 nonce=" + std::to_string(nonce));

    // 计算数据哈希
    auto data_hash = crypto::Sha256Hex(data);
    Dbg("data_hash = SHA256(data) = " + data_hash);

    Transaction tx;
    tx.type = "STORE_DATA";
    tx.from = sender.address;
    tx.to = "";
    tx.amount = 0;
    tx.data_hash = data_hash;
    tx.nonce = nonce;
    tx.timestamp = NowMillis();
    tx.public_key_hex = sender.public_key_hex;

    Dbg("构建 Transaction 对象:");
    Dbg("  type=STORE_DATA");
    Dbg("  from=" + tx.from);
    Dbg("  data_hash=" + tx.data_hash);
    Dbg("  nonce=" + std::to_string(tx.nonce));

    std::string error;
    if (!SubmitAndCommit(tx, error)) {
        PrintErr("交易失败: " + error);
        return;
    }

    uint64_t height = 0;
    auto latest = storage_.GetLatestBlock();
    if (latest) height = latest->header.height;
    TxRecord rec{tx.tx_id, tx.type, tx.from, "", 0, height};
    recent_tx_map_.Put(tx.tx_id, rec);
    tx_order_.push_back(tx.tx_id);

    PrintOk("数据存储成功! tx_id=" + tx.tx_id.substr(0, 16) + "...");
    std::cout << "  tx_id:    " << tx.tx_id << "\n";
    std::cout << "  区块高度: " << height << "\n";
    Dbg("=== STORE_DATA 完成 ===");
    Pause();
}

// ══════════════════════════════════════════════════════════════════════════════
// 提交交易并出块（核心流程，附带详细日志）
// ══════════════════════════════════════════════════════════════════════════════

bool CliSession::SubmitAndCommit(Transaction& tx, std::string& out_error) {
    // ── Step 1: 序列化交易体 ──
    auto tx_body = SerializeTransactionBody(tx);
    Dbg("Step 1 - 序列化 tx_body:");
    Dbg("  字段顺序: type|from|to|amount|data_hash|nonce|timestamp|public_key");
    Dbg("  tx_body = " + tx_body);

    // ── Step 2: 签名 ──
    tx.signature_hex = crypto::SignDetachedHex(tx_body, logged_in_users_[active_user_index_].private_key_hex);
    Dbg("Step 2 - Ed25519 签名:");
    Dbg("  signature_hex = " + tx.signature_hex);

    // ── Step 3: 计算 tx_id ──
    tx.tx_id = ComputeTransactionId(tx);
    Dbg("Step 3 - 计算 tx_id:");
    Dbg("  tx_id = SHA256(body + \"|\" + signature) = " + tx.tx_id);

    // ── Step 4: Mempool 入场检查 ──
    Dbg("Step 4 - Mempool 入场检查:");
    if (!mempool_.AddTransaction(tx, out_error)) {
        DbgPrint("  ✗ Mempool 拒绝: " + out_error);
        return false;
    }
    Dbg("  ✓ tx_id 非空");
    Dbg("  ✓ 无重复交易");
    Dbg("  ✓ 签名验证通过");
    Dbg("  ✓ 交易大小 OK (≤16KB)");
    DbgPrint("  交易已加入 mempool, tx_id=" + tx.tx_id.substr(0, 16) + "...");

    // ── Step 5: 记录共识事件 ──
    consensus_.AddEvent({0, 0, "", 0, 0, 0, "MEMPOOL_ADD", tx.from, config_.node_id, tx.tx_id, true, "", ""});
    Dbg("Step 5 - 共识事件 MEMPOOL_ADD 已记录");

    // 持久化交易为 PENDING
    storage_.PutTransaction(tx, "PENDING", std::nullopt, std::nullopt);

    // ── Step 6: 出块 ──
    if (consensus_.IsRunning() && consensus_.GetAttackMode() == AttackMode::NORMAL) {
        DbgPrint("Step 6 - 构建区块...");

        auto picked = mempool_.PickTransactions(100);
        Dbg("  从 mempool 取出 " + std::to_string(picked.size()) + " 笔交易");

        try {
            Block block;
            block.transactions = picked;
            block.header.chain_id = config_.chain_id;
            block.header.height = std::stoull(storage_.GetMetadata("latest_height", "0")) + 1;
            auto latest = storage_.GetLatestBlock();
            block.header.previous_block_hash = latest ? latest->header.block_hash : std::string(64, '0');

            Dbg("  height=" + std::to_string(block.header.height));
            Dbg("  previous_block_hash=" + block.header.previous_block_hash);

            // Merkle 树构建
            Dbg("  --- Merkle Tree 构建 ---");
            auto levels = MerkleTree::BuildLevels(picked);
            for (size_t lv = 0; lv < levels.size(); ++lv) {
                std::string hashes;
                for (size_t i = 0; i < levels[lv].size(); ++i) {
                    if (i > 0) hashes += ", ";
                    hashes += HashToHex(levels[lv][i]).substr(0, 16) + "...";
                }
                Dbg("    level[" + std::to_string(lv) + "]: [" + hashes + "]");
            }
            block.header.tx_merkle_root = HashToHex(MerkleTree::ComputeRoot(picked));
            Dbg("  tx_merkle_root = " + block.header.tx_merkle_root);

            // 执行交易计算 state_root
            Dbg("  --- 执行交易计算 state_root ---");
            for (size_t i = 0; i < picked.size(); ++i) {
                const auto& ptx = picked[i];
                Dbg("    tx[" + std::to_string(i) + "] type=" + ptx.type +
                    " from=" + ptx.from.substr(0, 12) + "..." +
                    " amount=" + std::to_string(ptx.amount) +
                    " nonce=" + std::to_string(ptx.nonce));
                if (ptx.type == "TRANSFER") {
                    auto from_before = storage_.GetAccount(ptx.from);
                    auto to_before = storage_.GetAccount(ptx.to);
                    Dbg("      from 余额: " +
                        (from_before ? std::to_string(from_before->balance) : "0") +
                        " → " + std::to_string(from_before ? from_before->balance - ptx.amount : 0));
                    Dbg("      to   余额: " +
                        (to_before ? std::to_string(to_before->balance) : "0") +
                        " → " + std::to_string(to_before ? to_before->balance + ptx.amount : ptx.amount));
                }
            }
            block.header.state_root = executor_.ExecuteForStateRoot(picked);
            Dbg("  state_root = " + block.header.state_root);

            block.header.timestamp = NowMillis();
            block.header.view = 0;
            block.header.instance_id = 0;
            block.header.proposer_id = config_.node_id;
            block.header.block_hash = ComputeBlockHash(block.header);

            Dbg("  proposer_id = " + block.header.proposer_id);
            Dbg("  block_hash = " + block.header.block_hash);

            // 序列化区块头
            Dbg("  --- 区块头序列化 ---");
            auto header_ser = SerializeBlockHeaderForHash(block.header);
            Dbg("  header_serialized = " + header_ser);

            // 提交区块
            executor_.CommitBlock(block);
            Dbg("  区块已提交到数据库");

            consensus_.AddEvent({0, 0, "", block.header.height, block.header.view, block.header.instance_id,
                                 "AUTO_COMMIT_BLOCK", config_.node_id, config_.node_id, block.header.block_hash, true, "", ""});
            mempool_.RemoveCommitted(picked);

            DbgPrint("Step 7 - 区块提交成功: height=" + std::to_string(block.header.height));
            return true;
        } catch (const std::exception& e) {
            // 出块失败: 清理 mempool 和 DB 中的 PENDING 交易
            Dbg("  出块失败: " + std::string(e.what()));
            Dbg("  回滚: 清理 mempool 和 PENDING 交易");
            mempool_.RemoveCommitted(picked);  // 从 mempool 移除
            // 从 DB 删除这些 PENDING 交易
            for (const auto& ptx : picked) {
                try {
                    // 直接执行 SQL 删除 PENDING 交易
                    sqlite3_stmt* del = nullptr;
                    sqlite3_prepare_v2(storage_.Raw(),
                        "DELETE FROM transactions WHERE tx_id=? AND status='PENDING';",
                        -1, &del, nullptr);
                    sqlite3_bind_text(del, 1, ptx.tx_id.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_step(del);
                    sqlite3_finalize(del);
                } catch (...) {}
            }
            out_error = std::string("出块失败: ") + e.what() + " (交易已回滚)";
            return false;
        }
    }

    DbgPrint("  共识引擎未运行或处于攻击模式，交易状态: PENDING");
    return true;
}

// ══════════════════════════════════════════════════════════════════════════════
// 5. 查询账户状态
// ══════════════════════════════════════════════════════════════════════════════

void CliSession::DoQueryAccount() {
    auto addr = PromptLine("请输入账户地址 (留空查询当前用户): ");
    if (addr.empty() && active_user_index_ >= 0) {
        addr = logged_in_users_[active_user_index_].address;
    }
    if (addr.empty()) {
        PrintErr("地址不能为空");
        return;
    }

    auto account = storage_.GetAccount(addr);
    if (!account) {
        PrintErr("账户不存在: " + addr);
        return;
    }

    DbgSep("查询账户状态");
    Dbg("address=" + account->address);
    Dbg("balance=" + std::to_string(account->balance));
    Dbg("nonce=" + std::to_string(account->nonce));

    std::cout << "\n  地址:   " << account->address << "\n";
    std::cout << "  余额:   " << account->balance << "\n";
    std::cout << "  Nonce:  " << account->nonce << "\n";
    Pause();
}

// ══════════════════════════════════════════════════════════════════════════════
// 6. 查询区块
// ══════════════════════════════════════════════════════════════════════════════

void CliSession::DoQueryBlock() {
    auto input = PromptLine("请输入区块高度 (留空查询最新): ");
    std::optional<Block> block;
    if (input.empty()) {
        block = storage_.GetLatestBlock();
    } else {
        block = storage_.GetBlockByHeight(std::stoull(input));
    }
    if (!block) {
        PrintErr("区块不存在");
        return;
    }

    DbgSep("查询区块 #" + std::to_string(block->header.height));
    Dbg("chain_id=" + block->header.chain_id);
    Dbg("height=" + std::to_string(block->header.height));
    Dbg("previous_block_hash=" + block->header.previous_block_hash);
    Dbg("tx_merkle_root=" + block->header.tx_merkle_root);
    Dbg("state_root=" + block->header.state_root);
    Dbg("block_hash=" + block->header.block_hash);
    Dbg("proposer_id=" + block->header.proposer_id);
    Dbg("交易数量=" + std::to_string(block->transactions.size()));

    std::cout << "\n  高度:       " << block->header.height << "\n";
    std::cout << "  区块哈希:   " << block->header.block_hash.substr(0, 32) << "...\n";
    std::cout << "  前一区块:   " << block->header.previous_block_hash.substr(0, 32) << "...\n";
    std::cout << "  Merkle根:   " << block->header.tx_merkle_root.substr(0, 32) << "...\n";
    std::cout << "  状态根:     " << block->header.state_root.substr(0, 32) << "...\n";
    std::cout << "  提议者:     " << block->header.proposer_id << "\n";
    std::cout << "  交易数:     " << block->transactions.size() << "\n";
    for (size_t i = 0; i < block->transactions.size(); ++i) {
        const auto& t = block->transactions[i];
        std::cout << "    [" << i << "] " << t.type << " " << t.from.substr(0, 8) << "... → "
                  << t.to.substr(0, 8) << "...  amount=" << t.amount << "\n";
    }
    Pause();
}

// ══════════════════════════════════════════════════════════════════════════════
// 7. 查询交易
// ══════════════════════════════════════════════════════════════════════════════

void CliSession::DoQueryTransaction() {
    auto tx_id = PickTxId("请输入交易ID (或最近记录编号): ");
    if (tx_id.empty()) {
        PrintErr("交易ID不能为空");
        return;
    }

    auto tx = storage_.GetTransaction(tx_id);
    if (!tx) {
        PrintErr("交易不存在");
        return;
    }

    DbgSep("查询交易");
    Dbg("tx_id=" + tx->tx_id);
    Dbg("type=" + tx->type);
    Dbg("from=" + tx->from);
    Dbg("to=" + tx->to);
    Dbg("amount=" + std::to_string(tx->amount));
    Dbg("nonce=" + std::to_string(tx->nonce));
    Dbg("timestamp=" + std::to_string(tx->timestamp));

    std::cout << "\n  tx_id:     " << tx->tx_id << "\n";
    std::cout << "  类型:      " << tx->type << "\n";
    std::cout << "  发送方:    " << tx->from << "\n";
    std::cout << "  接收方:    " << tx->to << "\n";
    std::cout << "  金额:      " << tx->amount << "\n";
    std::cout << "  Nonce:     " << tx->nonce << "\n";
    std::cout << "  时间戳:    " << tx->timestamp << "\n";
    Pause();
}

// ══════════════════════════════════════════════════════════════════════════════
// 8. 查询待处理交易
// ══════════════════════════════════════════════════════════════════════════════

void CliSession::DoQueryPending() {
    auto pending = mempool_.Pending();
    DbgSep("查询 mempool 待处理交易");
    Dbg("待处理交易数: " + std::to_string(pending.size()));

    std::cout << "\n  待处理交易数: " << pending.size() << "\n";
    for (size_t i = 0; i < pending.size(); ++i) {
        const auto& t = pending[i];
        std::cout << "  [" << i << "] " << t.tx_id.substr(0, 16) << "... "
                  << t.type << " " << t.from.substr(0, 8) << "... → "
                  << t.to.substr(0, 8) << "...  amount=" << t.amount << "\n";
    }
    Pause();
}

// ══════════════════════════════════════════════════════════════════════════════
// 9. Merkle 证明验证
// ══════════════════════════════════════════════════════════════════════════════

void CliSession::DoMerkleProof() {
    auto tx_id = PickTxId("请输入交易ID (或最近记录编号): ");
    if (tx_id.empty()) {
        PrintErr("交易ID不能为空");
        return;
    }

    auto tx = storage_.GetTransaction(tx_id);
    auto height = storage_.GetTransactionBlockHeight(tx_id);
    if (!tx || !height) {
        PrintErr("未找到已提交的交易");
        return;
    }
    auto block = storage_.GetBlockByHeight(*height);
    if (!block) {
        PrintErr("区块不存在");
        return;
    }

    // 找到交易在区块中的索引
    size_t index = 0;
    for (; index < block->transactions.size(); ++index) {
        if (block->transactions[index].tx_id == tx->tx_id) break;
    }

    DbgSep("Merkle 证明验证");
    DbgPrint("验证交易 " + tx->tx_id.substr(0, 16) + "... 的 Merkle 证明");
    Dbg("tx_id=" + tx->tx_id);
    Dbg("block_height=" + std::to_string(*height));
    Dbg("tx_index=" + std::to_string(index));

    // 叶子哈希
    auto leaf_hash = MerkleTree::LeafHash(*tx);
    Dbg("leaf_hash = Hash(0x00 || tx_serialized) = " + HashToHex(leaf_hash));

    // 生成证明
    auto proof = MerkleTree::GenerateProof(block->transactions, index);
    Dbg("证明路径 (" + std::to_string(proof.size()) + " 层):");
    for (size_t i = 0; i < proof.size(); ++i) {
        Dbg("  [" + std::to_string(i) + "] " +
            std::string(proof[i].position == MerkleProofItem::Position::LEFT ? "LEFT" : "RIGHT") +
            " sibling=" + HashToHex(proof[i].sibling_hash));
    }

    // 重算根哈希
    Dbg("--- 重算根哈希 ---");
    Hash current = leaf_hash;
    for (size_t lv = 0; lv < proof.size(); ++lv) {
        auto before = current;
        if (proof[lv].position == MerkleProofItem::Position::LEFT) {
            current = MerkleTree::ParentHash(proof[lv].sibling_hash, current);
            Dbg("  level[" + std::to_string(lv) + "]: LEFT  Hash(" +
                HashToHex(proof[lv].sibling_hash).substr(0, 12) + "... || " +
                HashToHex(before).substr(0, 12) + "...) = " +
                HashToHex(current).substr(0, 16) + "...");
        } else {
            current = MerkleTree::ParentHash(current, proof[lv].sibling_hash);
            Dbg("  level[" + std::to_string(lv) + "]: RIGHT Hash(" +
                HashToHex(before).substr(0, 12) + "... || " +
                HashToHex(proof[lv].sibling_hash).substr(0, 12) + "...) = " +
                HashToHex(current).substr(0, 16) + "...");
        }
    }

    bool valid = (HashToHex(current) == block->header.tx_merkle_root);
    Dbg("computed_root = " + HashToHex(current));
    Dbg("expected_root = " + block->header.tx_merkle_root);
    Dbg("验证结果: " + std::string(valid ? "✓ 通过" : "✗ 失败"));

    std::cout << "\n  交易ID:     " << tx->tx_id.substr(0, 24) << "...\n";
    std::cout << "  区块高度:   " << *height << "\n";
    std::cout << "  证明层数:   " << proof.size() << "\n";
    std::cout << "  计算根:     " << HashToHex(current).substr(0, 32) << "...\n";
    std::cout << "  期望根:     " << block->header.tx_merkle_root.substr(0, 32) << "...\n";
    if (valid) PrintOk("Merkle 证明验证通过!");
    else PrintErr("Merkle 证明验证失败!");
    Pause();
}

// ══════════════════════════════════════════════════════════════════════════════
// 10. SMT 状态证明
// ══════════════════════════════════════════════════════════════════════════════

void CliSession::DoSMTProof() {
    auto addr = PromptLine("请输入账户地址 (留空查询当前用户): ");
    if (addr.empty() && active_user_index_ >= 0) {
        addr = logged_in_users_[active_user_index_].address;
    }
    if (addr.empty()) {
        PrintErr("地址不能为空");
        return;
    }

    auto account = storage_.GetAccount(addr);
    if (!account) {
        PrintErr("账户不存在");
        return;
    }

    DbgSep("SMT 状态证明");
    DbgPrint("验证账户 " + account->address.substr(0, 12) + "... 的 SMT 存在性证明");

    // 构建 SMT
    SparseMerkleTree smt;
    auto key = crypto::Sha256String(account->address);
    auto value = EncodeAccountState(*account);
    smt.Update(key, value);

    Dbg("address=" + account->address);
    Dbg("key = SHA256(address) = " + HashToHex(key));
    Dbg("value = SerializeAccountState = " + SerializeAccountState(*account));
    Dbg("value_hash = SHA256(value) = " + HashToHex(crypto::Sha256(value)));

    // 生成存在性证明
    auto proof = smt.GenerateExistenceProof(key);
    Dbg("证明类型: EXISTENCE");
    Dbg("兄弟哈希 (" + std::to_string(proof.sibling_hashes.size()) + " 个):");

    // 路径位
    std::string path_bits;
    for (size_t i = 0; i < 8 && i < 256; ++i) {
        const size_t byte_index = i / 8;
        const size_t bit_index = 7 - (i % 8);
        path_bits.push_back(((key[byte_index] >> bit_index) & 1U) ? '1' : '0');
    }
    Dbg("路径前8位: " + path_bits);

    // 重算根哈希
    Dbg("--- 重算根哈希 (从叶子到根, 256层) ---");
    auto value_hash = crypto::Sha256(value);
    Hash current = SparseMerkleTree::LeafHash(key, value_hash);
    Dbg("  leaf_hash = " + HashToHex(current));

    for (size_t depth = 255; depth < 256; --depth) {
        const auto& sibling = proof.sibling_hashes[255 - depth];
        const size_t byte_index = depth / 8;
        const size_t bit_index = 7 - (depth % 8);
        bool bit = ((key[byte_index] >> bit_index) & 1U) == 1U;
        auto before = current;
        if (bit) {
            current = SparseMerkleTree::ParentHash(sibling, current);
        } else {
            current = SparseMerkleTree::ParentHash(current, sibling);
        }
        // 只打印前3层和最后3层
        if (depth >= 253 || depth <= 2) {
            Dbg("  depth[" + std::to_string(depth) + "] bit=" + std::to_string(bit ? 1 : 0) +
                " Hash(" + HashToHex(bit ? sibling : before).substr(0, 8) + "... || " +
                HashToHex(bit ? before : sibling).substr(0, 8) + "...) = " +
                HashToHex(current).substr(0, 12) + "...");
        }
    }
    Dbg("  ... (中间层省略) ...");

    auto root = smt.GetRoot();
    bool valid = (current == root);
    Dbg("computed_root = " + HashToHex(current));
    Dbg("expected_root = " + HashToHex(root));
    Dbg("验证结果: " + std::string(valid ? "✓ 通过" : "✗ 失败"));

    std::cout << "\n  地址:       " << account->address << "\n";
    std::cout << "  余额:       " << account->balance << "\n";
    std::cout << "  Nonce:      " << account->nonce << "\n";
    std::cout << "  SMT 根:     " << HashToHex(root).substr(0, 32) << "...\n";
    if (valid) PrintOk("SMT 存在性证明验证通过!");
    else PrintErr("SMT 存在性证明验证失败!");
    Pause();
}

// ══════════════════════════════════════════════════════════════════════════════
// 11. 攻击模拟
// ══════════════════════════════════════════════════════════════════════════════

void CliSession::DoAttackSimulation() {
    DbgSep("攻击模拟");

    std::cout << "\n  可用攻击模式:\n";
    std::cout << "  ┌────┬──────────────────────────┬────────────────────────────────┐\n";
    std::cout << "  │ 编号 │ 模式名称                   │ 说明                           │\n";
    std::cout << "  ├────┼──────────────────────────┼────────────────────────────────┤\n";
    std::cout << "  │  0 │ normal                   │ 正常模式                       │\n";
    std::cout << "  │  1 │ bad_merkle_root          │ 篡改 Merkle 根                 │\n";
    std::cout << "  │  2 │ bad_state_root           │ 篡改状态根                     │\n";
    std::cout << "  │  3 │ invalid_block_hash       │ 篡改区块哈希                   │\n";
    std::cout << "  │  4 │ invalid_node_signature    │ 伪造节点签名                   │\n";
    std::cout << "  │  5 │ drop_prepare             │ 丢弃 PREPARE 消息              │\n";
    std::cout << "  │  6 │ drop_commit              │ 丢弃 COMMIT 消息               │\n";
    std::cout << "  │  7 │ delay_preprepare         │ 延迟 PRE_PREPARE 消息          │\n";
    std::cout << "  │  8 │ double_proposal          │ 双重提议                       │\n";
    std::cout << "  │  9 │ node_crash_simulated     │ 模拟节点崩溃                   │\n";
    std::cout << "  │ 10 │ equivocation_prepare     │ 矛盾投票 (PREPARE)             │\n";
    std::cout << "  │ 11 │ replay_old_message       │ 重放旧消息                     │\n";
    std::cout << "  └────┴──────────────────────────┴────────────────────────────────┘\n";

    auto choice = PromptUint64("请选择攻击模式 [0-11]: ");

    AttackMode mode;
    std::string mode_name;
    switch (choice) {
        case 0:  mode = AttackMode::NORMAL; mode_name = "normal"; break;
        case 1:  mode = AttackMode::BAD_MERKLE_ROOT; mode_name = "bad_merkle_root"; break;
        case 2:  mode = AttackMode::BAD_STATE_ROOT; mode_name = "bad_state_root"; break;
        case 3:  mode = AttackMode::INVALID_BLOCK_HASH; mode_name = "invalid_block_hash"; break;
        case 4:  mode = AttackMode::INVALID_NODE_SIGNATURE; mode_name = "invalid_node_signature"; break;
        case 5:  mode = AttackMode::DROP_PREPARE; mode_name = "drop_prepare"; break;
        case 6:  mode = AttackMode::DROP_COMMIT; mode_name = "drop_commit"; break;
        case 7:  mode = AttackMode::DELAY_PREPREPARE; mode_name = "delay_preprepare"; break;
        case 8:  mode = AttackMode::DOUBLE_PROPOSAL; mode_name = "double_proposal"; break;
        case 9:  mode = AttackMode::NODE_CRASH_SIMULATED; mode_name = "node_crash_simulated"; break;
        case 10: mode = AttackMode::EQUIVOCATION_PREPARE; mode_name = "equivocation_prepare"; break;
        case 11: mode = AttackMode::REPLAY_OLD_MESSAGE; mode_name = "replay_old_message"; break;
        default: PrintErr("无效选择"); return;
    }

    consensus_.SetAttackMode(mode);
    Dbg("设置攻击模式: " + mode_name);
    PrintOk("攻击模式已设置为: " + mode_name);

    // 如果是 normal 模式，提示用户
    if (mode == AttackMode::NORMAL) {
        std::cout << "  已恢复正常模式，后续交易将正常出块。\n";
        Pause();
        return;
    }

    // 演示攻击效果
    if (active_user_index_ >= 0) {
        auto apply = PromptLine("是否提交一笔交易以观察攻击效果? (y/n): ");
        if (apply == "y" || apply == "Y") {
            Dbg("=== 攻击模式下提交交易 ===");
            const auto& sender = logged_in_users_[active_user_index_];
            auto account = storage_.GetAccount(sender.address);
            uint64_t nonce = account ? account->nonce + 1 : 1;

            // 构建一笔小额自转交易
            Transaction tx;
            tx.type = "TRANSFER";
            tx.from = sender.address;
            tx.to = sender.address;
            tx.amount = 1;
            tx.nonce = nonce;
            tx.timestamp = NowMillis();
            tx.public_key_hex = sender.public_key_hex;
            tx.data_hash = "";

            Dbg("构建攻击测试交易: self-transfer 1, nonce=" + std::to_string(nonce));

            // 序列化 + 签名
            auto tx_body = SerializeTransactionBody(tx);
            tx.signature_hex = crypto::SignDetachedHex(tx_body, sender.private_key_hex);
            tx.tx_id = ComputeTransactionId(tx);
            Dbg("tx_id=" + tx.tx_id);

            // 加入 mempool
            std::string error;
            if (!mempool_.AddTransaction(tx, error)) {
                Dbg("mempool 拒绝: " + error);
                PrintErr("交易被 mempool 拒绝: " + error);
                Pause();
                return;
            }
            Dbg("交易已加入 mempool");

            // 出块时攻击模式的效果
            Dbg("当前攻击模式: " + mode_name);
            switch (mode) {
                case AttackMode::BAD_MERKLE_ROOT:
                    Dbg("效果: 出块时 Merkle 根将被篡改为随机值，诚实节点验证会失败");
                    break;
                case AttackMode::BAD_STATE_ROOT:
                    Dbg("效果: 出块时 state_root 将被篡改，状态验证不一致");
                    break;
                case AttackMode::INVALID_BLOCK_HASH:
                    Dbg("效果: 出块时 block_hash 将被篡改，区块完整性校验失败");
                    break;
                case AttackMode::DOUBLE_PROPOSAL:
                    Dbg("效果: 同一高度将产生两个不同区块提议，触发 equivocation 检测");
                    break;
                case AttackMode::EQUIVOCATION_PREPARE:
                    Dbg("效果: 节点对同一高度发送矛盾的 PREPARE 投票");
                    break;
                default:
                    Dbg("效果: " + mode_name + " 攻击行为将在共识消息处理中体现");
                    break;
            }

            // 在 NORMAL 模式下会自动出块，但在攻击模式下行为不同
            if (consensus_.IsRunning()) {
                auto picked = mempool_.PickTransactions(100);
                if (!picked.empty()) {
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

                    // 根据攻击模式篡改数据或拦截
                    if (mode == AttackMode::BAD_MERKLE_ROOT) {
                        auto orig = block.header.tx_merkle_root;
                        block.header.tx_merkle_root = std::string(64, 'f');
                        Dbg("篡改 Merkle 根: " + orig + " → " + block.header.tx_merkle_root);
                    } else if (mode == AttackMode::BAD_STATE_ROOT) {
                        auto orig = block.header.state_root;
                        block.header.state_root = std::string(64, 'a');
                        Dbg("篡改 state_root: " + orig + " → " + block.header.state_root);
                    } else if (mode == AttackMode::INVALID_BLOCK_HASH) {
                        auto orig = block.header.block_hash;
                        block.header.block_hash = std::string(64, 'b');
                        Dbg("篡改 block_hash: " + orig + " → " + block.header.block_hash);
                    } else if (mode == AttackMode::INVALID_NODE_SIGNATURE) {
                        // 篡改 proposer_id 模拟伪造节点签名
                        auto orig = block.header.proposer_id;
                        block.header.proposer_id = "FAKE_NODE_X";
                        Dbg("伪造节点签名: proposer_id " + orig + " → " + block.header.proposer_id);
                    } else if (mode == AttackMode::DOUBLE_PROPOSAL) {
                        // 模拟双重提议: 记录两个不同区块到同一高度
                        Dbg("双重提议: 同一高度产生两个不同区块");
                        consensus_.AddEvent({0, 0, "", block.header.height, block.header.view, block.header.instance_id,
                                             "DOUBLE_PROPOSAL", config_.node_id, "PROPOSAL_A", block.header.block_hash, true, "", "double_proposal"});
                        Block block2 = block;
                        block2.header.block_hash = ComputeBlockHash(block.header); // 重新计算以获得不同哈希
                        block2.header.timestamp += 1;
                        block2.header.block_hash = ComputeBlockHash(block2.header);
                        consensus_.AddEvent({0, 0, "", block2.header.height, block2.header.view, block2.header.instance_id,
                                             "DOUBLE_PROPOSAL", config_.node_id, "PROPOSAL_B", block2.header.block_hash, true, "", "double_proposal"});
                        Dbg("提案A: " + block.header.block_hash);
                        Dbg("提案B: " + block2.header.block_hash);
                    } else if (mode == AttackMode::NODE_CRASH_SIMULATED) {
                        // 模拟节点崩溃: 不提交区块, 交易丢失
                        Dbg("节点崩溃模拟: 区块未提交, 交易丢失");
                        mempool_.RemoveCommitted(picked);
                        consensus_.AddEvent({0, 0, "", block.header.height, block.header.view, block.header.instance_id,
                                             "NODE_CRASH", config_.node_id, config_.node_id, block.header.block_hash, false, "节点崩溃, 区块丢失", "node_crash_simulated"});
                        PrintOk("节点崩溃模拟: 区块 #0" + std::to_string(block.header.height) + " 未提交 (交易丢失)");
                        Pause();
                        return;
                    } else if (mode == AttackMode::DROP_PREPARE || mode == AttackMode::DROP_COMMIT) {
                        // 模拟消息丢弃: 区块提交但共识事件标记为不接受
                        std::string phase = (mode == AttackMode::DROP_PREPARE) ? "PREPARE" : "COMMIT";
                        Dbg("丢弃 " + phase + " 消息: 区块提交但共识未达成");
                        consensus_.AddEvent({0, 0, "", block.header.height, block.header.view, block.header.instance_id,
                                             "DROP_" + phase, config_.node_id, config_.node_id, block.header.block_hash, false,
                                             phase + " 消息被恶意节点丢弃", mode_name});
                    } else if (mode == AttackMode::DELAY_PREPREPARE) {
                        // 模拟延迟: 记录延迟事件
                        Dbg("延迟 PRE_PREPARE: 模拟 5 秒延迟");
                        consensus_.AddEvent({0, 0, "", block.header.height, block.header.view, block.header.instance_id,
                                             "DELAY_PREPREPARE", config_.node_id, config_.node_id, block.header.block_hash, true,
                                             "PRE_PREPARE 被延迟 5 秒", "delay_preprepare"});
                    } else if (mode == AttackMode::EQUIVOCATION_PREPARE) {
                        // 模拟矛盾投票: 同一节点对两个不同区块投票
                        Dbg("矛盾投票: 节点对两个不同区块发送 PREPARE");
                        std::string hash1 = block.header.block_hash;
                        BlockHeader h2 = block.header;
                        h2.timestamp += 1;
                        std::string hash2 = ComputeBlockHash(h2);
                        consensus_.AddEvent({0, 0, "", block.header.height, block.header.view, block.header.instance_id,
                                             "EQUIVOCATION", config_.node_id, config_.node_id, hash1, true, "", "equivocation_prepare"});
                        consensus_.AddEvent({0, 0, "", block.header.height, block.header.view, block.header.instance_id,
                                             "EQUIVOCATION", config_.node_id, config_.node_id, hash2, true, "", "equivocation_prepare"});
                        Dbg("投票1: " + hash1);
                        Dbg("投票2: " + hash2);
                    } else if (mode == AttackMode::REPLAY_OLD_MESSAGE) {
                        // 模拟重放: 记录旧消息事件
                        Dbg("重放旧消息: 尝试重放 height=1 的消息");
                        consensus_.AddEvent({0, 0, "", 1, 0, 0,
                                             "REPLAY", config_.node_id, config_.node_id, "old_block_hash_000000", false,
                                             "重放旧区块消息被检测到", "replay_old_message"});
                    }

                    Dbg("区块构建完成: height=" + std::to_string(block.header.height) +
                        " merkle_root=" + block.header.tx_merkle_root +
                        " state_root=" + block.header.state_root +
                        " block_hash=" + block.header.block_hash);

                    // ── Step 1: 快照受影响账户, 提交区块 (模拟提议) ──
                    Dbg("Step 1 - 恶意节点提议区块...");
                    struct AccSnap { std::string addr; uint64_t balance; uint64_t nonce; };
                    std::vector<AccSnap> snapshot;
                    for (const auto& ptx : picked) {
                        auto acc = storage_.GetAccount(ptx.from);
                        if (acc) snapshot.push_back({acc->address, acc->balance, acc->nonce});
                        if (!ptx.to.empty()) {
                            auto acc2 = storage_.GetAccount(ptx.to);
                            if (acc2) snapshot.push_back({acc2->address, acc2->balance, acc2->nonce});
                        }
                    }
                    executor_.CommitBlock(block);
                    mempool_.RemoveCommitted(picked);
                    Dbg("  区块已暂存, 账户快照已保存");

                    // ── Step 2: 诚实节点验证区块 (模拟共识验证) ──
                    Dbg("Step 2 - 诚实节点验证区块...");
                    std::cout << "\n  ── 共识验证 ──\n";
                    bool verified = true;
                    std::string reject_reason;

                    // 验证 Merkle 根
                    auto real_merkle = HashToHex(MerkleTree::ComputeRoot(block.transactions));
                    if (real_merkle != block.header.tx_merkle_root) {
                        verified = false;
                        reject_reason = "Merkle 根不匹配";
                        Dbg("  ✗ Merkle 根: 期望=" + real_merkle + " 实际=" + block.header.tx_merkle_root);
                        std::cout << "  真实 Merkle 根:  " << real_merkle.substr(0, 32) << "...\n";
                        std::cout << "  区块 Merkle 根: " << block.header.tx_merkle_root.substr(0, 32) << "...\n";
                    }
                    // 验证 state_root
                    if (verified) {
                        auto real_state = executor_.ExecuteForStateRoot(block.transactions);
                        if (real_state != block.header.state_root) {
                            verified = false;
                            reject_reason = "state_root 不匹配";
                            Dbg("  ✗ state_root: 期望=" + real_state + " 实际=" + block.header.state_root);
                            std::cout << "  真实 state_root:  " << real_state.substr(0, 32) << "...\n";
                            std::cout << "  区块 state_root: " << block.header.state_root.substr(0, 32) << "...\n";
                        }
                    }
                    // 验证 block_hash
                    if (verified) {
                        BlockHeader hdr = block.header;
                        std::string saved = hdr.block_hash;
                        hdr.block_hash = "";
                        if (ComputeBlockHash(hdr) != saved) {
                            verified = false;
                            reject_reason = "block_hash 不匹配";
                            Dbg("  ✗ block_hash 验证失败");
                        }
                    }
                    // 验证 proposer_id
                    if (verified) {
                        bool known = (block.header.proposer_id == config_.node_id);
                        if (!known) for (const auto& p : config_.peers) if (p.node_id == block.header.proposer_id) { known = true; break; }
                        if (!known) {
                            verified = false;
                            reject_reason = "未知节点 proposer_id=" + block.header.proposer_id;
                            Dbg("  ✗ proposer: " + block.header.proposer_id);
                        }
                    }

                    // ── Step 3: 验证结果 ──
                    if (!verified) {
                        Dbg("Step 3 - 验证失败, 回滚: " + reject_reason);
                        std::cout << "  \033[31m✗ 验证失败: " << reject_reason << "!\033[0m\n";
                        std::cout << "  区块 #" << block.header.height << " 被拒绝, 回滚交易\n";

                        // 回滚区块
                        storage_.Begin();
                        try {
                            sqlite3_stmt* d1 = nullptr;
                            sqlite3_prepare_v2(storage_.Raw(), "DELETE FROM blocks WHERE height=?;", -1, &d1, nullptr);
                            sqlite3_bind_int64(d1, 1, static_cast<sqlite3_int64>(block.header.height));
                            sqlite3_step(d1); sqlite3_finalize(d1);

                            for (const auto& ptx : picked) {
                                sqlite3_stmt* u = nullptr;
                                sqlite3_prepare_v2(storage_.Raw(),
                                    "UPDATE transactions SET status='PENDING', block_height=NULL, tx_index=NULL WHERE tx_id=?;",
                                    -1, &u, nullptr);
                                sqlite3_bind_text(u, 1, ptx.tx_id.c_str(), -1, SQLITE_TRANSIENT);
                                sqlite3_step(u); sqlite3_finalize(u);
                            }
                            storage_.PutMetadata("latest_height", std::to_string(block.header.height - 1));

                            // 恢复账户快照
                            for (const auto& s : snapshot) {
                                storage_.PutAccount({s.addr, s.balance, s.nonce}, 0);
                            }
                            storage_.Commit();
                            Dbg("  回滚完成: 区块已删除, 账户已恢复");
                        } catch (...) {
                            storage_.Rollback();
                            Dbg("  回滚异常");
                        }

                        PrintErr("区块 #" + std::to_string(block.header.height) + " 被共识拒绝: " + reject_reason);
                        consensus_.AddEvent({0, 0, "", block.header.height, block.header.view, block.header.instance_id,
                                             "ATTACK_BLOCK_REJECTED", config_.node_id, config_.node_id, block.header.block_hash, false, reject_reason, mode_name});
                    } else {
                        Dbg("Step 3 - 验证通过, 区块确认");
                        PrintOk("区块 #" + std::to_string(block.header.height) + " 验证通过");
                        consensus_.AddEvent({0, 0, "", block.header.height, block.header.view, block.header.instance_id,
                                             "ATTACK_BLOCK_ACCEPTED", config_.node_id, config_.node_id, block.header.block_hash, true, "", mode_name});
                    }
                }
            }
        }
    }

    std::cout << "\n  提示: 使用菜单选项 9 (Merkle 证明) 或 12 (节点状态) 观察攻击效果。\n";
    Dbg("=== 攻击模拟设置完成 ===");
    Pause();
}

// ══════════════════════════════════════════════════════════════════════════════
// 15. 查看已注册用户 (管理)
// ══════════════════════════════════════════════════════════════════════════════

void CliSession::DoListUsers() {
    DbgSep("查看已注册用户");
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(storage_.Raw(),
        "SELECT u.user_id, u.username, u.address, u.public_key, u.private_key_encrypted, "
        "COALESCE(a.balance, 0), COALESCE(a.nonce, 0) "
        "FROM users u LEFT JOIN accounts a ON u.address = a.address "
        "ORDER BY u.user_id;",
        -1, &stmt, nullptr);

    std::cout << "\n  ── 已注册用户 ──\n";
    std::cout << "  ID │ 用户名       │ 地址               │ 余额   │ Nonce\n";
    std::cout << "  ───┼──────────────┼────────────────────┼────────┼──────\n";

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int64_t uid = sqlite3_column_int64(stmt, 0);
        std::string uname = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        std::string addr = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        uint64_t balance = static_cast<uint64_t>(sqlite3_column_int64(stmt, 5));
        uint64_t nonce = static_cast<uint64_t>(sqlite3_column_int64(stmt, 6));

        std::cout << "  " << std::setw(2) << uid << " │ "
                  << std::setw(12) << uname << " │ "
                  << addr.substr(0, 18) << ".. │ "
                  << std::setw(6) << balance << " │ "
                  << std::setw(5) << nonce << "\n";
        count++;
    }
    sqlite3_finalize(stmt);

    if (count == 0) {
        std::cout << "  (暂无用户)\n";
    }

    std::cout << "\n";
    std::cout << "  安全说明:\n";
    std::cout << "  • 密码: Argon2id 单向哈希, 无法反推\n";
    std::cout << "  • 私钥: XSalsa20-Poly1305 加密存储, 密钥从密码派生\n";
    std::cout << "  • 忘记密码 → 私钥无法解密 → 只能重新注册\n";
    Dbg("用户总数: " + std::to_string(count));
    Pause();
}

// ══════════════════════════════════════════════════════════════════════════════
// 14. 最近交易记录
// ══════════════════════════════════════════════════════════════════════════════

void CliSession::DoShowHistory() {
    if (tx_order_.empty()) {
        std::cout << "\n  暂无交易记录。请先执行转账或存储数据操作。\n";
        Pause();
        return;
    }
    std::cout << "\n  ── 最近交易记录 (哈希表存储, O(1) 查找) ──\n";
    std::cout << "  编号 │ 类型         │ 发送方         │ 接收方         │ 金额  │ 区块  │ tx_id\n";
    std::cout << "  ─────┼──────────────┼────────────────┼────────────────┼───────┼───────┼──────────────────\n";
    for (size_t i = 0; i < tx_order_.size(); ++i) {
        auto rec = recent_tx_map_.Get(tx_order_[i]);
        if (!rec) continue;
        std::cout << "  " << std::setw(4) << i << " │ "
                  << std::setw(12) << rec->type << " │ "
                  << rec->from.substr(0, 12) << ".. │ "
                  << (rec->to.empty() ? "-" : rec->to.substr(0, 12) + "..") << " │ "
                  << std::setw(5) << rec->amount << " │ "
                  << std::setw(5) << rec->block_height << " │ "
                  << rec->tx_id.substr(0, 16) << "...\n";
    }
    std::cout << "\n  存储: CustomHashTable (链地址法, 负载因子>0.75时自动扩容)\n";
    std::cout << "  当前: " << recent_tx_map_.Size() << " 条记录, "
              << recent_tx_map_.BucketCount() << " 个桶, "
              << "负载因子=" << std::fixed << std::setprecision(2) << recent_tx_map_.LoadFactor() << "\n";
    std::cout << "  提示: 在查询交易(7)或 Merkle 证明(9)时可输入编号快速选择。\n";
    Pause();
}

std::string CliSession::PickTxId(const std::string& prompt) {
    if (!tx_order_.empty()) {
        std::cout << "  最近交易:\n";
        for (size_t i = 0; i < tx_order_.size(); ++i) {
            auto rec = recent_tx_map_.Get(tx_order_[i]);
            if (!rec) continue;
            std::cout << "    [" << i << "] " << rec->type
                      << " " << rec->tx_id.substr(0, 16)
                      << "... height=" << rec->block_height << "\n";
        }
    }
    auto input = PromptLine(prompt);
    if (input.empty()) return "";
    // 尝试解析为编号
    try {
        size_t idx = std::stoull(input);
        if (idx < tx_order_.size()) {
            return tx_order_[idx];
        }
    } catch (...) {}
    // 否则当作 tx_id, 用哈希表 O(1) 验证是否存在
    if (recent_tx_map_.Contains(input)) {
        return input;
    }
    // 用户直接输入的 tx_id 可能不在历史中, 仍返回让调用方查询 DB
    return input;
}

// ══════════════════════════════════════════════════════════════════════════════
// 12. 节点状态
// ══════════════════════════════════════════════════════════════════════════════

void CliSession::DoNodeStatus() {
    auto height = storage_.GetMetadata("latest_height", "0");

    std::cout << "\n  ── 节点状态 ──\n";
    std::cout << "  节点ID:     " << config_.node_id << "\n";
    std::cout << "  REST端口:   " << config_.rest_port << "\n";
    std::cout << "  P2P端口:    " << config_.p2p_port << "\n";
    std::cout << "  链ID:       " << config_.chain_id << "\n";
    std::cout << "  最新高度:   " << height << "\n";
    std::cout << "  f:          " << config_.f << "\n";
    std::cout << "  实例数:     " << config_.instance_count << "\n";
    std::cout << "  quorum:     " << consensus_.Quorum() << "\n";
    std::cout << "  已登录用户: " << logged_in_users_.size() << "\n";
    for (size_t i = 0; i < logged_in_users_.size(); ++i) {
        std::cout << "    [" << i << "] " << logged_in_users_[i].username
                  << "  " << logged_in_users_[i].address.substr(0, 12) << "...\n";
    }
    Pause();
}

// ══════════════════════════════════════════════════════════════════════════════
// 13. 共识状态
// ══════════════════════════════════════════════════════════════════════════════

void CliSession::DoConsensusStatus() {
    auto status = consensus_.Status();
    auto events = consensus_.RecentEvents(10);

    std::cout << "\n  ── 共识状态 ──\n";
    std::cout << "  运行状态:   " << (consensus_.IsRunning() ? "运行中" : "已停止") << "\n";
    std::cout << "  攻击模式:   " << AttackModeToString(consensus_.GetAttackMode()) << "\n";
    std::cout << "  Quorum:     " << consensus_.Quorum() << "\n";

    DbgSep("共识状态查询");
    Dbg("running=" + std::string(consensus_.IsRunning() ? "true" : "false"));
    Dbg("attack_mode=" + AttackModeToString(consensus_.GetAttackMode()));
    Dbg("quorum=" + std::to_string(consensus_.Quorum()));

    if (!events.empty()) {
        std::cout << "\n  最近事件:\n";
        for (const auto& e : events) {
            std::cout << "    [" << e.height << "] " << e.event_type
                      << " from=" << e.from.substr(0, 8)
                      << " accepted=" << (e.accepted ? "Y" : "N") << "\n";
        }
    }
    Pause();
}

} // namespace rbft
