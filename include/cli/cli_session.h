#pragma once

#include "block/block_executor.h"
#include "common/config.h"
#include "consensus/consensus.h"
#include "merkle/merkle_tree.h"
#include "storage/sqlite_storage.h"
#include "tx/mempool.h"
#include "user/user_manager.h"

#include "datastructure/custom_hash_table.h"

#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace rbft {

// 已登录用户会话信息
struct CliUserSession {
    std::string username;
    std::string address;
    std::string public_key_hex;
    std::string private_key_hex;
    std::string token;
};

// 最近交易记录
struct TxRecord {
    std::string tx_id;
    std::string type;       // TRANSFER / STORE_DATA
    std::string from;
    std::string to;
    uint64_t amount{};
    uint64_t block_height{};
};

class CliSession {
public:
    explicit CliSession(const NodeConfig& config);
    ~CliSession();

    // 禁止拷贝
    CliSession(const CliSession&) = delete;
    CliSession& operator=(const CliSession&) = delete;

    // 交互式主循环
    void Run();

private:
    // ── 菜单操作 ──
    void DoRegister();
    void DoLogin();
    void DoTransfer();
    void DoStoreData();
    void ShowMenu() const;

    // ── 查询 ──
    void DoQueryAccount();
    void DoQueryBlock();
    void DoQueryTransaction();
    void DoQueryPending();

    // ── 证明 ──
    void DoMerkleProof();
    void DoSMTProof();

    // ── 攻击模拟 ──
    void DoAttackSimulation();

    // ── 历史记录 ──
    void DoShowHistory();

    // ── 管理 ──
    void DoListUsers();

    // ── 系统状态 ──
    void DoNodeStatus();
    void DoConsensusStatus();

    // ── 辅助: 选择最近交易 ──
    std::string PickTxId(const std::string& prompt);

    // ── 辅助 ──
    std::string PromptLine(const std::string& prompt) const;
    uint64_t PromptUint64(const std::string& prompt) const;
    void PrintOk(const std::string& msg) const;
    void PrintErr(const std::string& msg) const;
    void Pause() const;

    // ── 调试日志 ──
    void OpenDebugLog();
    void Dbg(const std::string& msg);          // 仅写入日志文件
    void DbgPrint(const std::string& msg);     // 同时写入日志文件和终端
    void DbgSep(const std::string& title);

    // ── 提交交易并出块（核心流程，附带详细日志）──
    bool SubmitAndCommit(Transaction& tx, std::string& out_error);

    // ── 内部状态 ──
    NodeConfig config_;
    SQLiteStorage storage_;
    std::unique_ptr<UserManager> users_;

    Mempool mempool_;
    ConsensusEngine consensus_;
    BlockExecutor executor_;

    std::vector<CliUserSession> logged_in_users_;   // 已登录用户列表
    int active_user_index_{-1};                      // 当前活跃用户索引

    // 最近交易记录: 哈希表用于 O(1) 按 tx_id 查找, tx_order_ 维护插入顺序用于展示
    CustomHashTable<std::string, TxRecord> recent_tx_map_{64};
    std::vector<std::string> tx_order_;

    std::ofstream debug_log_;                        // 调试日志文件流
};

} // namespace rbft
