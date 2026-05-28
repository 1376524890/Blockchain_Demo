# Git 工作流规范

## 分支规范

```text
main：稳定分支
dev：开发分支
feature/xxx：功能分支
fix/xxx：修复分支
test/xxx：测试分支
```

所有功能先进入 `dev`，通过单元测试、集成测试和攻击测试后再合并 `main`。

## Commit Message 规范

```text
feat: add merkle proof api
fix: reject invalid state root
test: add double proposal attack test
docs: update smt design
refactor: split consensus module
chore: update cmake
```

类型：

- `feat`：新增功能。
- `fix`：缺陷修复。
- `test`：测试相关。
- `docs`：文档更新。
- `refactor`：不改变行为的重构。
- `chore`：构建、脚本、依赖和杂项。

## 目录管理规范

- `include/`：公开头文件。
- `src/`：实现文件。
- `tests/unit/`：GoogleTest 单元测试。
- `tests/integration/`：Python/curl 集成和攻击测试。
- `docs/`：需求、架构、设计和部署文档。
- `scripts/`：构建、启动、测试和 demo 脚本。
- `data/`：本地运行数据，不提交数据库和日志。

## 代码风格规范

- C++17。
- 类名 PascalCase，函数名 PascalCase 或现有模块风格一致，变量名 snake_case。
- 每个核心类拆分 `.h` 与 `.cpp`。
- 哈希、签名、区块头和交易必须使用确定性序列化。
- 网络输入必须校验类型、长度和必填字段。
- 不吞异常，REST 层捕获异常并返回统一错误响应。

## Pull Request 检查项

- 文档是否与代码行为一致。
- 是否新增或更新单元测试。
- 是否影响数据库 schema。
- 是否影响 REST API 响应格式。
- 是否通过 `./scripts/build.sh`。
- 是否通过 `./scripts/run_unit_tests.sh`。
- 共识、Merkle、SMT、交易签名变更是否有攻击测试覆盖。

## 版本号规范

```text
v0.1.0：基础工程和文档
v0.2.0：用户和交易
v0.3.0：Merkle Tree
v0.4.0：SMT
v0.5.0：简化 RBFT
v0.6.0：REST API
v0.7.0：攻击测试
v1.0.0：完整演示版
```
