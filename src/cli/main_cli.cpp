#include "cli/cli_session.h"
#include "common/config.h"
#include "common/logger.h"
#include "crypto/crypto_utils.h"

#include <exception>
#include <filesystem>
#include <iostream>

static void PrintBanner() {
    std::cout << "\n";
    std::cout << "\033[36m  ██████╗ ██████╗ ███████╗████████╗\033[0m\n";
    std::cout << "\033[36m  ██╔══██╗██╔══██╗██╔════╝╚══██╔══╝\033[0m\n";
    std::cout << "\033[36m  ██████╔╝██████╔╝█████╗     ██║   \033[0m\n";
    std::cout << "\033[36m  ██╔══██╗██╔══██╗██╔══╝     ██║   \033[0m\n";
    std::cout << "\033[36m  ██║  ██║██║  ██║██║        ██║   \033[0m\n";
    std::cout << "\033[36m  ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝        ╚═╝   \033[0m\n";
    std::cout << "\033[1m  Chain Demo - 交互式命令行客户端\033[0m\n";
    std::cout << "\n";
}

static void PrintUsage(const char* prog) {
    std::cout << "用法: " << prog << " [选项]\n";
    std::cout << "选项:\n";
    std::cout << "  --config <path>  指定节点配置文件 (默认: config/node1.json)\n";
    std::cout << "  --help           显示帮助信息\n";
}

int main(int argc, char** argv) {
    try {
        std::string config_path;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--config" && i + 1 < argc) {
                config_path = argv[++i];
            } else if (arg == "--help" || arg == "-h") {
                PrintUsage(argv[0]);
                return 0;
            }
        }

        // 自动定位配置文件: 优先用户指定, 否则按 当前目录 → 上级目录 搜索
        if (config_path.empty()) {
            if (std::filesystem::exists("config/node1.json")) {
                config_path = "config/node1.json";
            } else if (std::filesystem::exists("../config/node1.json")) {
                config_path = "../config/node1.json";
            } else {
                throw std::runtime_error(
                    "找不到配置文件 config/node1.json\n"
                    "请在项目根目录或 build 目录下运行, 或使用 --config <path> 指定");
            }
        }

        // 初始化 libsodium
        rbft::crypto::Init();

        // 加载配置
        auto config = rbft::LoadConfig(config_path);

        // 切换工作目录到项目根目录 (如果从 build/ 运行)
        // 这样 data/ 和 config/ 的相对路径都能正确解析
        auto config_abs = std::filesystem::absolute(config_path);
        auto project_root = config_abs.parent_path().parent_path(); // config/node1.json → project_root
        if (std::filesystem::exists(project_root / "CMakeLists.txt")) {
            std::filesystem::current_path(project_root);
        }

        // 设置日志
        std::filesystem::create_directories("data/" + config.node_id);
        rbft::Logger::SetLogFile("data/" + config.node_id + "/cli.log");

        PrintBanner();
        std::cout << "  节点: " << config.node_id << "  端口: " << config.rest_port << "\n";
        std::cout << "  数据库: " << config.db_path << "\n";
        std::cout << "  调试日志: data/" << config.node_id << "/cli_debug.log\n";
        std::cout << "\n";

        // 创建并运行 CLI 会话
        rbft::CliSession session(config);
        session.Run();

        std::cout << "\n\033[33m再见!\033[0m\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\033[31m[致命错误]\033[0m " << e.what() << "\n";
        return 1;
    }
}
