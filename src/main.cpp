#include "api/api_server.h"
#include "common/config.h"
#include "common/logger.h"

#include <exception>
#include <filesystem>
#include <iostream>

int main(int argc, char** argv) {
    try {
        std::string config_path = "config/node1.json";
        bool init_db_only = false;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--config" && i + 1 < argc) {
                config_path = argv[++i];
            } else if (arg == "--init-db") {
                init_db_only = true;
            }
        }
        auto cfg = rbft::LoadConfig(config_path);
        std::filesystem::create_directories(std::filesystem::path(cfg.db_path).parent_path());
        rbft::Logger::SetLogFile("data/" + cfg.node_id + "/node.log");
        rbft::Logger::Info("starting " + cfg.node_id + " rest=" + std::to_string(cfg.rest_port) +
                           " p2p=" + std::to_string(cfg.p2p_port));
        if (init_db_only) {
            rbft::ApiServer app(cfg);
            app.InitDbOnly();
            rbft::Logger::Info("database initialized");
            return 0;
        }
        rbft::ApiServer app(cfg);
        app.Run();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "fatal: " << e.what() << "\n";
        return 1;
    }
}
