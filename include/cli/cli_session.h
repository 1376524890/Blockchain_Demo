#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace httplib { class Client; }

namespace rbft {

// 远程节点连接
struct RemoteNode {
    std::string node_id;
    std::string host;
    int port{};
    std::unique_ptr<httplib::Client> client;
};

// 钱包信息 (本地存储)
struct Wallet {
    std::string username;
    std::string address;
    std::string public_key;
    std::string private_key;     // 明文 (从加密文件解密后)
    std::string encrypted_key;   // 加密后的私钥 (存储在文件中)
};

// 已注册用户 (从远程节点获取)
struct RemoteUser {
    std::string username;
    std::string address;
    std::string public_key;
    std::string private_key;
    std::string token;
};

// 最近交易记录
struct TxRecord {
    std::string tx_id;
    std::string type;
    std::string from;
    std::string to;
    uint64_t amount{};
    std::string status;     // COMMITTED / PENDING / REJECTED
};

class CliSession {
public:
    explicit CliSession(const std::vector<std::pair<std::string, int>>& node_endpoints);
    ~CliSession();

    CliSession(const CliSession&) = delete;
    CliSession& operator=(const CliSession&) = delete;

    void Run();

private:
    // ── 菜单 ──
    void ShowMenu() const;

    // ── 操作 ──
    void DoRegister();
    void DoLogin();
    void DoTransfer();
    void DoStoreData();
    void DoQueryAccount();
    void DoQueryBlock();
    void DoQueryTransaction();
    void DoQueryPending();
    void DoMerkleProof();
    void DoSMTProof();
    void DoAttackSimulation();
    void DoNodeStatus();
    void DoConsensusStatus();
    void DoShowHistory();
    void DoListUsers();
    void DoConsensusVerify();

    // ── HTTP 辅助 ──
    bool HttpGet(int node_idx, const std::string& path, std::string& out_body);
    bool HttpPost(int node_idx, const std::string& path, const std::string& json_body, std::string& out_body);
    bool ParseOk(const std::string& body, std::string& out_data, std::string& out_error);

    // ── 多节点广播 ──
    // 向所有节点发送相同请求, 返回每个节点的响应
    struct NodeResponse {
        int node_idx;
        bool ok;
        std::string data;
        std::string error;
    };
    std::vector<NodeResponse> BroadcastGet(const std::string& path);
    std::vector<NodeResponse> BroadcastPost(const std::string& path, const std::string& json_body);
    NodeResponse SendToNode(int node_idx, const std::string& method, const std::string& path, const std::string& body = "");

    // ── 辅助 ──
    std::string PromptLine(const std::string& prompt) const;
    uint64_t PromptUint64(const std::string& prompt) const;
    void PrintOk(const std::string& msg) const;
    void PrintErr(const std::string& msg) const;
    void Pause() const;
    int PickNode(const std::string& prompt) const;

    // ── 钱包管理 ──
    void InitWalletDir();
    bool SaveWallet(const std::string& username, const std::string& password,
                    const std::string& address, const std::string& public_key, const std::string& private_key);
    bool LoadWallet(const std::string& username, const std::string& password, Wallet& out);

    // ── 调试日志 ──
    void OpenDebugLog();
    void Dbg(const std::string& msg);
    void DbgPrint(const std::string& msg);
    void DbgSep(const std::string& title);

    // ── 内部状态 ──
    std::vector<RemoteNode> nodes_;
    std::vector<RemoteUser> users_;
    int active_user_{-1};

    std::vector<TxRecord> recent_txs_;
    std::vector<std::string> tx_order_;

    std::ofstream debug_log_;
    std::string log_path_;
    std::string wallet_dir_;
};

} // namespace rbft
