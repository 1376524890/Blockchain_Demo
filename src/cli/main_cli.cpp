#include "cli/cli_session.h"

#include <exception>
#include <iostream>
#include <string>
#include <vector>

static void PrintBanner() {
    std::cout << "\n";
    std::cout << "\033[36m  ██████╗ ██████╗ ███████╗████████╗\033[0m\n";
    std::cout << "\033[36m  ██╔══██╗██╔══██╗██╔════╝╚══██╔══╝\033[0m\n";
    std::cout << "\033[36m  ██████╔╝██████╔╝█████╗     ██║   \033[0m\n";
    std::cout << "\033[36m  ██╔══██╗██╔══██╗██╔══╝     ██║   \033[0m\n";
    std::cout << "\033[36m  ██║  ██║██║  ██║██║        ██║   \033[0m\n";
    std::cout << "\033[36m  ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝        ╚═╝   \033[0m\n";
    std::cout << "\033[1m  Chain Demo - 多节点共识客户端\033[0m\n";
    std::cout << "\n";
}

static void PrintUsage(const char* prog) {
    std::cout << "用法: " << prog << " [选项]\n";
    std::cout << "选项:\n";
    std::cout << "  --nodes host:p1,p2,p3,p4  指定节点地址和端口 (默认: localhost:8001,8002,8003,8004)\n";
    std::cout << "  --help                    显示帮助信息\n";
    std::cout << "\n";
    std::cout << "示例:\n";
    std::cout << "  " << prog << "\n";
    std::cout << "  " << prog << " --nodes 192.168.1.10:8001,8002,8003,8004\n";
}

int main(int argc, char** argv) {
    try {
        std::string host = "localhost";
        std::vector<int> ports = {8001, 8002, 8003, 8004};

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--nodes" && i + 1 < argc) {
                std::string val = argv[++i];
                auto colon = val.find(':');
                if (colon != std::string::npos) {
                    host = val.substr(0, colon);
                    std::string ports_str = val.substr(colon + 1);
                    ports.clear();
                    size_t pos = 0;
                    while (pos < ports_str.size()) {
                        auto comma = ports_str.find(',', pos);
                        if (comma == std::string::npos) comma = ports_str.size();
                        ports.push_back(std::stoi(ports_str.substr(pos, comma - pos)));
                        pos = comma + 1;
                    }
                }
            } else if (arg == "--help" || arg == "-h") {
                PrintUsage(argv[0]);
                return 0;
            }
        }

        // 构建节点端点列表
        std::vector<std::pair<std::string, int>> endpoints;
        for (size_t i = 0; i < ports.size(); ++i) {
            endpoints.push_back({host, ports[i]});
        }

        PrintBanner();
        std::cout << "  连接节点:\n";
        for (size_t i = 0; i < endpoints.size(); ++i) {
            std::cout << "    node" << (i + 1) << " → " << endpoints[i].first << ":" << endpoints[i].second << "\n";
        }
        std::cout << "\n";

        rbft::CliSession session(endpoints);
        session.Run();

        std::cout << "\n\033[33m再见!\033[0m\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\033[31m[致命错误]\033[0m " << e.what() << "\n";
        return 1;
    }
}
