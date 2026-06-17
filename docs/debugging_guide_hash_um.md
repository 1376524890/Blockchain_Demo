# RBFT Chain Demo — 通过 CLI 交互调试演示用户管理 & 哈希表原理

## 目录

1. [架构概览：调用链路](#1-架构概览调用链路)
2. [前置准备：编译 Debug 版本](#2-前置准备编译-debug-版本)
3. [VS Code 调试配置](#3-vs-code-调试配置)
4. [断点规划总览](#4-断点规划总览)
5. [演示流程 Step-by-Step](#5-演示流程-step-by-step)
6. [GDB 命令行备选方案](#6-gdb-命令行备选方案)
7. [常见问题排查](#7-常见问题排查)

---

## 1. 架构概览：调用链路

```
┌──────────────────────────────────────────────────────────────────────┐
│  终端 A: VS Code 调试器 (GDB)                                         │
│  ┌────────────────────────────────────────────────────────────────┐  │
│  │  rbft_node (Debug build)                                       │  │
│  │  ├─ ApiServer::RegisterRoutes()                                │  │
│  │  │   └─ POST /api/users/register                               │  │
│  │  │       └─ users_.Register(username, password)  ──────────┐   │  │
│  │  └─ UserManager users_                                       │   │  │
│  │       ├─ username_index_ : CustomHashTable<string, int64_t>  │   │  │
│  │       └─ address_index_  : CustomHashTable<string, int64_t>  │   │  │
│  │            │                                                  │   │  │
│  │            └─ Put() / Contains() / Get()                      │   │  │
│  │                 └─ Index(key) → std::hash % buckets           │   │  │
│  │                 └─ Separate Chaining (链地址法)               │   │  │
│  │                 └─ Rehash() 扩容 (LoadFactor > 0.75)          │   │  │
│  └────────────────────────────────────────────────────────────────┘  │
│                    ↑ HTTP :8001                                       │
├────────────────────┼──────────────────────────────────────────────────┤
│  终端 B: CLI 客户端 │                                                  │
│  ┌─────────────────┴──────────────────────────────────────────────┐  │
│  │  rbft_cli                                                      │  │
│  │  菜单 1. 注册用户 → POST /api/users/register                   │  │
│  │  菜单 2. 用户登录 → 钱包解密 + 节点验证                         │  │
│  └────────────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────────────┘
```

**关键文件**：
| 文件 | 角色 |
|------|------|
| `src/api/api_server.cpp:275-296` | API 端点：接收 CLI 的注册请求 |
| `src/api/api_server.cpp:298-307` | API 端点：接收 CLI 的登录请求 |
| `src/user/user_manager.cpp:13-58` | 核心：Register 逻辑 + 哈希表索引 |
| `src/user/user_manager.cpp:103-141` | 核心：Login 逻辑 |
| `include/datastructure/custom_hash_table.h:17-32` | 哈希表 Put（含扩容判断） |
| `include/datastructure/custom_hash_table.h:70-83` | 哈希表 Rehash（扩容） |
| `include/datastructure/custom_hash_table.h:101-103` | Index()：哈希函数 + 取模 |

---

## 2. 前置准备：编译 Debug 版本

```bash
cd /home/marktom/rbft_chain_demo

# 清理旧构建
rm -rf out/build/linux-debug
mkdir -p out/build/linux-debug

# CMake 配置（Debug + 完整符号表 + 禁用优化）
cmake -S . -B out/build/linux-debug \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-g3 -O0"

# 编译所有目标（节点 + CLI + 单元测试）
cmake --build out/build/linux-debug -j$(nproc)
```

验证：
```bash
file out/build/linux-debug/rbft_node
# 期望: "with debug_info, not stripped"

file out/build/linux-debug/rbft_cli
# 期望: "with debug_info, not stripped"
```

---

## 3. VS Code 调试配置

### 3.1 launch.json（替换 .vscode/launch.json）

```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "name": "🔬 调试 rbft_node (CLI交互演示)",
      "type": "cppdbg",
      "request": "launch",
      "program": "${workspaceFolder}/out/build/linux-debug/rbft_node",
      "args": [
        "--config",
        "config/node1.json"
      ],
      "cwd": "${workspaceFolder}",
      "stopAtEntry": false,
      "environment": [],
      "externalConsole": false,
      "MIMode": "gdb",
      "miDebuggerPath": "/usr/bin/gdb",
      "setupCommands": [
        {
          "description": "Enable pretty-printing for STL containers",
          "text": "-enable-pretty-printing",
          "ignoreFailures": true
        },
        {
          "description": "Unlimited print elements",
          "text": "set print elements 0",
          "ignoreFailures": true
        },
        {
          "description": "Pretty print",
          "text": "set print pretty on",
          "ignoreFailures": true
        },
        {
          "description": "Follow fork mode",
          "text": "set follow-fork-mode child",
          "ignoreFailures": true
        }
      ]
    },
    {
      "name": "🎯 独立哈希表演示 (demo_hash_user)",
      "type": "cppdbg",
      "request": "launch",
      "program": "${workspaceFolder}/out/build/linux-debug/rbft_demo_hash",
      "args": [],
      "cwd": "${workspaceFolder}",
      "stopAtEntry": true,
      "environment": [],
      "externalConsole": false,
      "MIMode": "gdb",
      "miDebuggerPath": "/usr/bin/gdb",
      "setupCommands": [
        { "description": "Enable pretty-printing", "text": "-enable-pretty-printing", "ignoreFailures": true },
        { "description": "Unlimited print", "text": "set print elements 0", "ignoreFailures": true },
        { "description": "Pretty print", "text": "set print pretty on", "ignoreFailures": true }
      ]
    }
  ]
}
```

### 3.2 双终端布局

```
┌──────────────────────┬──────────────────────────┐
│  VS Code (终端A)      │  终端 B (独立 terminal)   │
│                      │                          │
│  F5 启动调试          │  cd /home/marktom/       │
│  选择 "🔬 调试        │    rbft_chain_demo       │
│  rbft_node"           │                          │
│                      │  ./out/build/            │
│  设置断点             │    linux-debug/rbft_cli   │
│                      │                          │
│  等待断点命中 ←───────┤  输入菜单选项 1 (注册)     │
│  单步调试 →          │  输入用户名和密码          │
│  讲解哈希表原理       │                          │
└──────────────────────┴──────────────────────────┘
```

---

## 4. 断点规划总览

按**讲解顺序**排列，每个断点都通过 CLI 操作触发：

| # | 文件:行号 | 触发方式 | 讲解主题 | 优先级 |
|---|----------|---------|---------|--------|
| ① | `custom_hash_table.h:20` | CLI发送注册请求 → Put被调用 | 负载因子判断 & 扩容条件 | ⭐⭐⭐ |
| ② | `custom_hash_table.h:23` | 同上，F11进入Index() | 哈希函数 std::hash + 取模 | ⭐⭐⭐ |
| ③ | `custom_hash_table.h:30` | 同上 | 链地址法：push_back 追加 | ⭐⭐⭐ |
| ④ | `custom_hash_table.h:72` | 插入足够多用户触发扩容 | Rehash：翻倍扩容 & 重新散列 | ⭐⭐⭐ |
| ⑤ | `user_manager.cpp:20` | CLI发送注册请求 | Contains() 索引查重 | ⭐⭐⭐ |
| ⑥ | `user_manager.cpp:51` | 注册成功后 | username_index_.Put() | ⭐⭐⭐ |
| ⑦ | `user_manager.cpp:52` | 同上 | address_index_.Put() 双索引 | ⭐⭐⭐ |
| ⑧ | `api_server.cpp:275` | CLI发送注册请求 | API入口：从HTTP到UserManager | ⭐⭐ |
| ⑨ | `user_manager.cpp:105` | CLI登录 | Login SQL查询流程 | ⭐⭐ |
| ⑩ | `user_manager.cpp:117` | 同上 | Argon2id 密码验证 | ⭐⭐ |

---

## 5. 演示流程 Step-by-Step

### 阶段 0：启动调试 + 设置断点

**操作步骤**：

1. 在 VS Code 中打开项目
2. 按 `Ctrl+Shift+D` 打开 Run and Debug 面板
3. 在 **BREAKPOINTS** 面板中，添加以下断点（直接输入文件名和行号）：

```
custom_hash_table.h:20     ← 断点①
custom_hash_table.h:23     ← 断点②
custom_hash_table.h:30     ← 断点③
custom_hash_table.h:72     ← 断点④
user_manager.cpp:20        ← 断点⑤
user_manager.cpp:51        ← 断点⑥
user_manager.cpp:52        ← 断点⑦
api_server.cpp:275         ← 断点⑧
user_manager.cpp:105       ← 断点⑨
user_manager.cpp:117       ← 断点⑩
```

> **提示**：可以先禁用 (disable) 断点④和⑨⑩，用到时再启用，避免过早中断。

4. 在 **WATCH** 面板中添加：
```
users_.username_index_.buckets_
users_.username_index_.size_
users_.address_index_.buckets_
users_.address_index_.size_
```

5. 按 `F5` 选择 **"🔬 调试 rbft_node (CLI交互演示)"**
6. 等待终端输出 `listening REST/P2P on port 8001`，表示服务已启动

7. 在另一个终端中启动 CLI：
```bash
cd /home/marktom/rbft_chain_demo
./out/build/linux-debug/rbft_cli --nodes localhost:8001
```
看到菜单界面即就绪。

---

### 阶段 1：用户注册 → 哈希表 Put 流程（约 15 分钟）

这是**最核心的演示环节**，CLI 的一次注册请求会触发完整的哈希表操作链路。

#### Step 1.1 — 断点⑧：API 入口

**在 CLI 终端中操作**：输入 `1`（注册用户），输入用户名 `alice`，密码 `secret123`

**断点⑧ 命中** — `api_server.cpp:275`：
```cpp
server.Post("/api/users/register", [this](const httplib::Request& req, httplib::Response& res) {
```

**讲解**：
> "CLI 发送了 HTTP POST 请求到 `/api/users/register`。这里是 API 入口。
> 注意 `users_` 是 `UserManager` 实例，它内部维护了两个 `CustomHashTable` 做内存索引。"

**调试操作**：按 `F10` 逐步走到 `user_manager.cpp` 的 Register 调用（约 5-8 步），然后 `F11` 进入。

---

#### Step 1.2 — 断点⑤：Contains 索引查重（重要！）

**断点⑤ 命中** — `user_manager.cpp:20`：
```cpp
if (username_index_.Contains(username)) {
```

**讲解**：
> "这是**哈希表的第一个实际应用：快速查重**。
> `username_index_` 是 `CustomHashTable<std::string, int64_t>`，key=用户名，value=user_id。
> `Contains("alice")` 内部会调用 `Get()` → `Index()` 计算桶编号 → 在链中遍历查找。
> 由于是首次注册，哈希表为空，`Contains` 返回 false。"

**调试操作**：
- 按 `F11` Step Into 进入 `Contains` → `Get`，观察完整查找路径
- 在 DEBUG CONSOLE 中执行：
  ```
  -exec p this->username_index_.buckets_
  ```
  观察当前所有桶都是空的

> **对观众强调**：如果是第二次注册 `alice`，会命中 `throw std::runtime_error("用户名已存在")`。

---

#### Step 1.3 — 断点①：LoadFactor 扩容判断

> 在 Step 1.2 的 Continue 后，Register 继续执行。当执行到 `username_index_.Put(username, user_id)` 时进入哈希表。

**断点① 命中** — `custom_hash_table.h:20`：
```cpp
if (LoadFactor() > 0.75) {
```

**讲解**：
> "这是 `Put` 函数的第一行——**负载因子检查**。`LoadFactor() = size_ / buckets_.size()`。
> `username_index_` 初始化时有 **64 个桶**（构造函数指定），这是第一个元素，1/64 ≈ 0.016，远小于 0.75。
> **不会触发扩容**。为什么要 64 个初始桶？因为预计用户数量不会太多，64 个桶足够，避免频繁扩容。"

**调试操作**：
- 在 WATCH 面板观察 `size_` 和 `buckets_.size()` 的值
- 鼠标悬停在 `LoadFactor()` 上查看返回值
- `F10` 跳过 if 体（未触发扩容）

---

#### Step 1.4 — 断点②：Index() 哈希函数（重要！）

**断点② 命中** — `custom_hash_table.h:23`：
```cpp
auto& bucket = buckets_[Index(key)];
```

**讲解**：
> "现在计算 key 对应的桶编号。`F11` Step Into 进入 `Index()` 函数：
>
> ```cpp
> size_t Index(const K& key) const {
>     return std::hash<K>{}(key) % buckets_.size();
> }
> ```
>
> 两步操作：
> 1. **`std::hash<std::string>{}(key)`** — C++ 标准库对字符串做哈希，输出一个 64 位整数
> 2. **`% 64`** — 取模运算，把哈希值映射到 [0, 63] 的桶编号
>
> 这是哈希表 O(1) 查找的基础：不需要遍历 64 个桶，直接通过哈希函数算出目标桶。"

**调试操作**：
- `F11` 进入 `Index()` 函数
- 在 DEBUG CONSOLE 中执行：
  ```
  -exec p std::hash<std::string>{}(key)
  ```
  查看原始哈希值（例如：`14233153140131382490`）
- `F10` 越过取模，查看 `14233153140131382490 % 64 = ?`
- `Shift+F11` Step Out 返回

---

#### Step 1.5 — 断点③：链地址法处理碰撞（重要！）

**断点③ 命中** — `custom_hash_table.h:30`：
```cpp
bucket.push_back(Entry{key, value});
++size_;
```

**讲解**：
> "这是 `push_back` — 新元素追加到桶的链尾。前面的 `for` 循环遍历了桶内的链，确认 key 不存在后，走到这里追加。
>
> **链地址法（Separate Chaining）**：每个桶是一个 `std::vector<Entry>`。
> - 如果两个不同的 key 哈希到同一个桶（碰撞），第二个元素追加到链尾
> - 查找时先 O(1) 定位桶，再在链内做 O(链长) 的比较
> - 负载因子控制在 0.75 以下，链长通常 ≤ 1-2，查找近似 O(1)
>
> 当前是第一个元素，链从空变为 1 个节点。`size_` 从 0 变为 1。"

**调试操作**：
- 在 `push_back` 前观察 `bucket` 为空
- `F10` 越过 `push_back`，观察 `bucket` 现在有 1 个 Entry
- 观察 `size_` 变为 1

---

#### Step 1.6 — 断点⑥⑦：双索引写入（核心演示！）

> 继续执行，回到 `user_manager.cpp`。

**断点⑥ 命中** — `user_manager.cpp:51`：
```cpp
username_index_.Put(username, user_id);
```

**断点⑦ 命中** — `user_manager.cpp:52`：
```cpp
address_index_.Put(address, user_id);
```

**讲解**：
> "注册成功后，**同时写入两个哈希表索引**：
>
> | 哈希表 | Key | Value | 用途 |
> |--------|-----|-------|------|
> | `username_index_` | `"alice"` | `user_id=1` | 注册时查重、登录时快速验证 |
> | `address_index_` | `"a1b2c3..."` (40位hex) | `user_id=1` | 通过链上地址查找用户 |
>
> 这是**多索引模式** — 相当于数据库中的**二级索引（Secondary Index）**。
> 两个哈希表有不同的 key（一个用户名、一个地址哈希），但 value 都是同一个 user_id。
>
> 注意索引写入在 `storage_->Commit()` **之后** — 先确保持久化成功，再更新内存索引。"

**调试操作**：
- 在断点⑥处，在 DEBUG CONSOLE 中执行：
  ```
  -exec p username_index_.buckets_
  ```
  看到已有 1 个条目（第一次 Put 的结果）
- `F10` 越过，`size_` 不变（因为 Register 内的 Put 已经写入了 username_index_... 等等，不对）

> **注意**：Register 流程中，`username_index_.Put(username, user_id)` 在 `storage_->Commit()` 之后（第51行），但 `Contains` 在第20行。所以这里 `Put` 是新写入。如果之前哈希表为空，`Put` 后 `size_` 从 0→1。

让我重新核实一下：Register 的流程是：
1. 第20行 Contains 检查 - username_index_ 此时为空，返回 false
2. 第24-50行 密码学操作 + SQLite 写入 + Commit
3. 第51行 username_index_.Put(username, user_id) - 第一个 Put
4. 第52行 address_index_.Put(address, user_id) - 第二个 Put

所以断点⑥ 是 username_index_ 第一个 Put — 这是对的。

在 DEBUG CONSOLE 中：
```
-exec p this->username_index_.size_    # 断点⑥前: 0, 断点⑥后: 1
-exec p this->address_index_.size_     # 断点⑦前: 0, 断点⑦后: 1
```

---

### 阶段 2：观察 Rehash 扩容（约 5 分钟）

由于 `username_index_` 初始 64 个桶，阈值 0.75 × 64 = 48 个元素才触发扩容。手动注册 48+ 个用户太慢。

**快速触发 Rehash 的方法**：有两个选择：

#### 方法 A：使用独立演示程序（推荐）

1. 在 VS Code 中切换到 **"🎯 独立哈希表演示 (demo_hash_user)"** 配置
2. `F5` 启动，在 `stopAtEntry: true` 处暂停
3. 在 `custom_hash_table.h:72` 设置断点（Rehash 入口）
4. 按 `F5` Continue，程序在 `CustomHashTable<std::string, int> ht(4)` 上操作
5. 因为初始只有 4 个桶，第 4 个 Put 就会触发 Rehash

> 演示程序 `src/demo_hash_user.cpp` 已经包含了这部分的完整逻辑。

#### 方法 B：在 CLI 调试中，使用 DEBUG CONSOLE 手动调用

在 rbft_node 的调试过程中，在任意断点处，通过 DEBUG CONSOLE：
```
-exec p users_.username_index_.Rehash(8)
```
手动触发扩容并观察过程。

---

#### Step 2.1 — 断点④：Rehash 扩容

**断点④ 命中** — `custom_hash_table.h:72`：
```cpp
auto old = std::move(buckets_);
```

**讲解**：
> "**Rehash（重新散列）**是哈希表演示的重点。扩容分三步：
>
> 1. **Move 旧桶**（当前行）：`buckets_` 的内容被移动到 `old`，`buckets_` 变为空
> 2. **创建新桶**（下一行）：`buckets_.assign(8, {})` — 8 个空桶
> 3. **重新散列**（下面的循环）：遍历所有旧元素，重新 `Put` 到新桶
>
> 为什么要重新 Put？因为桶数从 4 变成 8，同一个 key 的 `hash % 4` 和 `hash % 8` 结果不同。"

**调试操作**：
- `F10` 越过 `auto old = std::move(buckets_);`
  - 在 DEBUG CONSOLE：`-exec p old` — 看到 4 个旧桶及其中的元素
  - `-exec p buckets_` — 看到 buckets_ 已被移空
- `F10` 越过 `buckets_.assign(8, {});`
  - `-exec p buckets_.size()` — 输出 8
- 在 Put 循环中 `F10`，观察每个元素逐个插入新桶

**关键对比**（在 DEBUG CONSOLE 中）：
```
# 扩容前（旧桶）
-exec p std::hash<std::string>{}("charlie") % 4

# 扩容后（新桶）
-exec p std::hash<std::string>{}("charlie") % 8
```
两个结果可能不同 — 这就是"重新散列"的含义。

**总结**：
> "扩容的时间复杂度是 O(n)，但因为每次翻倍，均摊到每次插入仍然是 O(1)。这跟 `std::vector` 的动态扩容是同一个道理。"

---

### 阶段 3：第二次注册 → 演示碰撞 & 查重（约 5 分钟）

**CLI 操作**：再次输入 `1`（注册），用户名 `alice`，密码随意。

#### Step 3.1 — 断点⑤ 再次命中：查重命中

**断点⑤ 再次命中** — `user_manager.cpp:20`：
```cpp
if (username_index_.Contains(username)) {
```

**讲解**：
> "这次 `username_index_` 中已经有 `alice` 了。`F11` 进入 `Contains` → `Get`，观察：
> 1. `Index("alice")` 计算桶编号
> 2. 在桶内的链中遍历，`entry.key == "alice"` 匹配成功
> 3. 返回 `true`
>
> **对比**：第一次注册时这里返回 false（哈希表为空），现在返回 true（已有记录）。
> 这就是内存索引带来的**亚微秒级查重**能力。"

**调试操作**：
- `F11` 进入 Contains → Get，逐步观察查找过程
- 退出后看到 `throw std::runtime_error("用户名 \"alice\" 已存在")`
- CLI 终端显示红色的错误信息

---

### 阶段 4：用户登录演示（约 5 分钟）

**CLI 操作**：输入 `2`（登录），用户名 `alice`，密码 `secret123`。

#### Step 4.1 — 断点⑨：Login SQL 查询

**断点⑨ 命中** — `user_manager.cpp:105`：
```cpp
sqlite3_prepare_v2(storage_->Raw(),
    "SELECT user_id,password_hash,address,public_key,private_key_encrypted FROM users WHERE username=?;",
    -1, &stmt, nullptr);
```

**讲解**：
> "登录时**直接查 SQLite**，不走哈希表。为什么？
> - 哈希表只存了 `username → user_id` 的**最小索引**
> - 登录需要完整记录：password_hash、public_key、encrypted_private_key
> - 这些敏感数据**不应该**全部缓存在内存哈希表中"

**调试操作**：
- `F10` 越过 prepare 和 bind
- `F10` 到 `sqlite3_step`，在 DEBUG CONSOLE 中手动查看返回的列值

#### Step 4.2 — 断点⑩：密码验证

**断点⑩ 命中** — `user_manager.cpp:117`：
```cpp
if (!crypto::PasswordVerify(password_hash, password)) {
```

**讲解**：
> "`PasswordVerify` 使用 **Argon2id** 算法（libsodium 的 `crypto_pwhash_str_verify`）。
> Argon2id 是 2015 年 Password Hashing Competition 的冠军，抗 GPU/ASIC 暴力破解。
>
> 密码正确则继续，解密私钥，生成 session token 返回给客户端。"

**调试操作**：
- `F10` 越过，验证通过，不进入 if 体
- 在 CLI 终端看到登录成功的提示

---

## 6. GDB 命令行备选方案

如果你更喜欢纯命令行的调试体验，可以不使用 VS Code：

### 终端 A：启动 GDB 调试 rbft_node

```bash
cd /home/marktom/rbft_chain_demo
gdb ./out/build/linux-debug/rbft_node

# GDB 初始化
(gdb) set print elements 0
(gdb) set print pretty on
(gdb) set follow-fork-mode child

# 一次性设置所有断点
(gdb) break custom_hash_table.h:20
(gdb) break custom_hash_table.h:23
(gdb) break custom_hash_table.h:30
(gdb) break custom_hash_table.h:72
(gdb) break user_manager.cpp:20
(gdb) break user_manager.cpp:51
(gdb) break user_manager.cpp:52
(gdb) break user_manager.cpp:105
(gdb) break user_manager.cpp:117

# 启动
(gdb) run --config config/node1.json
# 等待输出: "listening REST/P2P on port 8001"
```

### 终端 B：启动 CLI

```bash
cd /home/marktom/rbft_chain_demo
./out/build/linux-debug/rbft_cli --nodes localhost:8001
```

### GDB 常用调试命令速查

```bash
# 控制命令
n              # next: 单步越过（Step Over）
s              # step: 单步进入（Step Into）
finish         # 执行完当前函数并返回（Step Out）
c              # continue: 继续到下一个断点
bt             # backtrace: 打印调用栈
info locals    # 查看所有局部变量
info break     # 列出所有断点
disable 3      # 禁用断点 #3
enable 3       # 启用断点 #3
delete 1       # 删除断点 #1

# 打印命令
p users_.username_index_.buckets_        # 查看用户名哈希表所有桶
p users_.username_index_.size_            # 元素数
p users_.username_index_.LoadFactor()     # 负载因子
p users_.address_index_.buckets_          # 查看地址哈希表所有桶
p key                                     # 当前 key 值
p std::hash<std::string>{}(key)           # 原始哈希值
p username                                # 用户名
p user_id                                 # 用户ID

# 观察点
watch users_.username_index_.size_        # 当 size_ 变化时自动暂停
```

---

## 7. 常见问题排查

### 7.1 变量显示 `<optimized out>`

**解决**：重新编译时确保使用 `-O0`：
```bash
cmake -S . -B out/build/linux-debug -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-g3 -O0"
cmake --build out/build/linux-debug -j$(nproc)
```

### 7.2 模板类（CustomHashTable）断点无法设置

**现象**：VS Code 显示断点为灰色空心圆。

**解决**：
- 方法 1：先不设断点，在 `user_manager.cpp` 的调用处 `F11` Step Into 进入模板代码
- 方法 2：用 GDB 命令行 `break custom_hash_table.h:20`
- 方法 3：确认 CMakeLists.txt 中 rbft_core 编译时包含了模板实例化（头文件被 `#include` 即可，不需要额外配置）

### 7.3 调试器无法 attach 到 rbft_node

**现象**：launch 后程序闪退或无法连接。

**排查**：
```bash
# 确认端口未被占用
lsof -i :8001

# 确认 data 目录存在
ls -la data/node1/

# 手动启动（不带调试器）确认程序正常
./out/build/linux-debug/rbft_node --config config/node1.json
```

### 7.4 CLI 连接不上节点

**现象**：CLI 显示节点离线。

**排查**：
- 确认 rbft_node 已启动且输出 `listening REST/P2P on port 8001`
- 确认调试器没有在断点处暂停（CLI 的 HTTP 请求会超时）
- 在 CLI 操作前先按 `F5` Continue 让服务器运行

### 7.5 Pretty-Printer 不显示 STL 容器内容

**解决**：在 DEBUG CONSOLE 中手动执行：
```
-exec p users_.username_index_.buckets_
-exec p users_.username_index_.buckets_[0]
-exec p users_.username_index_.buckets_[0][0].key
-exec p users_.username_index_.buckets_[0][0].value
```

---

## 附录 A：演示时间线（30 分钟）

| 时间 | 阶段 | 断点 | CLI 操作 | 讲解要点 |
|------|------|------|---------|---------|
| 0-2min | 环境准备 | — | — | 编译 Debug，启动 VS Code + 终端 |
| 2-5min | 设置断点 + 启动 | — | — | 解释断点布局，启动 rbft_node |
| 5-8min | API入口 | ⑧ | CLI注册 alice | 调用链路：HTTP → ApiServer → UserManager |
| 8-12min | **哈希表核心** | ⑤①②③ | （同一请求） | Contains→Put→Index→hash函数→链地址法 |
| 12-15min | **双索引** | ⑥⑦ | （同一请求） | 两个哈希表同时写入 |
| 15-20min | **Rehash扩容** | ④ | demo_hash_user | 翻倍扩容，重新散列，均摊O(1) |
| 20-23min | **碰撞查重** | ⑤ | CLI再次注册 alice | 演示 Contains 命中，拒绝重复注册 |
| 23-28min | 登录流程 | ⑨⑩ | CLI登录 alice | SQLite查询 + Argon2id验证 |
| 28-30min | Q&A | — | — | — |

## 附录 B：关键 Watch 表达式速查

在 VS Code WATCH 面板中逐行添加：

```
# ===== 哈希表状态 =====
users_.username_index_.buckets_            # 嵌套vector，每个bucket是一个链
users_.username_index_.size_               # 总元素数
users_.username_index_.buckets_.size()     # 桶数量
users_.username_index_.LoadFactor()        # 负载因子

# ===== 地址索引状态 =====
users_.address_index_.buckets_
users_.address_index_.size_

# ===== 单个请求变量 =====
username                                   # 当前注册的用户名
user_id                                    # 分配的用户ID
address                                    # 生成的链上地址
```

---

> 核心文件：`include/datastructure/custom_hash_table.h`、`include/user/user_manager.h`、`src/user/user_manager.cpp`、`src/api/api_server.cpp`
>
> 演示入口：
> - 服务端：`F5` → "🔬 调试 rbft_node (CLI交互演示)"
> - 客户端：`./out/build/linux-debug/rbft_cli --nodes localhost:8001`
