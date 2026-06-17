// 调试演示用独立入口 —— 不依赖网络/共识，纯粹演示哈希表和用户管理逻辑
// 编译: cmake --build out/build/linux-debug --target rbft_demo_hash
// 调试: 在 VS Code 中选择 "🎯 哈希表+用户管理演示" 配置，按 F5

#include "datastructure/custom_hash_table.h"
#include "user/user_manager.h"
#include "storage/sqlite_storage.h"

#include <iostream>

int main() {
    // ===== 第一部分：哈希表数据结构原理演示 =====
    std::cout << "========== 哈希表数据结构原理演示 ==========\n" << std::endl;

    // ★ 观察点A: 初始状态 —— buckets_ 有4个空桶，size_=0
    rbft::CustomHashTable<std::string, int> ht(4);
    std::cout << "[初始] 桶数=" << ht.BucketCount() << ", 元素数=" << ht.Size()
              << ", 负载因子=" << ht.LoadFactor() << std::endl;

    // 插入3个元素 —— 负载因子 3/4=0.75，不触发扩容
    ht.Put("alice", 100);
    std::cout << "[Put alice] 负载因子=" << ht.LoadFactor() << std::endl;

    ht.Put("bob", 200);
    std::cout << "[Put bob] 负载因子=" << ht.LoadFactor() << std::endl;

    // ★ 观察点B: 3个元素分布在4个桶中，可能有碰撞
    ht.Put("charlie", 300);
    std::cout << "[Put charlie] 负载因子=" << ht.LoadFactor() << std::endl;

    // ★★★ 关键观察点C: 第4个元素触发 Rehash！
    // 负载因子 4/4=1.0 > 0.75 → 扩容到8个桶
    ht.Put("dave", 400);
    std::cout << "[Put dave → 触发Rehash] 桶数=" << ht.BucketCount()
              << ", 负载因子=" << ht.LoadFactor() << std::endl;

    // 验证查找
    int val;
    if (ht.Get("alice", val)) {
        std::cout << "[Get alice] value=" << val << std::endl;
    }
    std::cout << "[Contains eve] " << (ht.Contains("eve") ? "true" : "false") << std::endl;

    // ★ 观察点D: Remove 操作
    ht.Remove("bob");
    std::cout << "[Remove bob] 元素数=" << ht.Size()
              << ", 负载因子=" << ht.LoadFactor() << std::endl;
    std::cout << "[Contains bob after Remove] " << (ht.Contains("bob") ? "true" : "false") << std::endl;

    // ===== 第二部分：用户管理完整链路演示 =====
    std::cout << "\n========== 用户管理完整链路演示 ==========\n" << std::endl;

    // 初始化内存数据库
    rbft::SQLiteStorage storage;
    storage.Open(":memory:");
    storage.InitializeSchema();

    // ★ 观察点E: UserManager 构造 —— username_index_ 和 address_index_ 初始为空
    rbft::UserManager um(&storage);
    std::cout << "[初始化] UserManager 就绪，哈希表索引为空" << std::endl;

    // ★★★ 关键观察点F: 注册第一个用户 —— 双索引建立
    auto user1 = um.Register("alice_user", "secret123");
    std::cout << "[注册1] username=" << user1.username
              << ", user_id=" << user1.user_id
              << ", address=" << user1.address << std::endl;

    // ★ 观察点G: 注册第二个用户 —— 索引增长
    auto user2 = um.Register("bob_user", "strong456");
    std::cout << "[注册2] username=" << user2.username
              << ", user_id=" << user2.user_id
              << ", address=" << user2.address << std::endl;

    // ★ 观察点H: 重复注册 —— username_index_.Contains() 查重
    std::cout << "[尝试重复注册 alice_user] ";
    try {
        um.Register("alice_user", "whatever");
    } catch (const std::exception& e) {
        std::cout << "预期异常: " << e.what() << std::endl;
    }

    // ★ 观察点I: 登录 —— SQLite 回表查询 + 密码验证
    auto result = um.Login("alice_user", "secret123");
    std::cout << "[登录 alice_user] token=" << result.token.substr(0, 32) << "..."
              << ", user_id=" << result.user.user_id << std::endl;

    // 错误密码登录
    std::cout << "[尝试错误密码登录] ";
    try {
        um.Login("alice_user", "wrong_password");
    } catch (const std::exception& e) {
        std::cout << "预期异常: " << e.what() << std::endl;
    }

    // ★ 观察点J: GetByAddress —— 地址索引查询
    auto found = um.GetByAddress(user1.address);
    if (found.has_value()) {
        std::cout << "[GetByAddress] 找到用户: " << found->username
                  << " (id=" << found->user_id << ")" << std::endl;
    }

    std::cout << "\n========== 演示完成 ==========" << std::endl;
    return 0;
}
