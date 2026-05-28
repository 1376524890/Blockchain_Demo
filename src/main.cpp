#include "api/api_server.h"
#include "common/config.h"
#include "common/logger.h"
#include "crypto/crypto_utils.h"

#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>

int main(int argc, char** argv) {
    try {
        std::string config_path = "config/node1.json";
        bool init_db_only = false;
        std::string gen_key_path;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--config" && i + 1 < argc) {
                config_path = argv[++i];
            } else if (arg == "--init-db") {
                init_db_only = true;
            } else if (arg == "--gen-node-key" && i + 1 < argc) {
                gen_key_path = argv[++i];
            }
        }
        if (!gen_key_path.empty()) {
            auto kp = rbft::crypto::GenerateEd25519KeyPair();
            std::filesystem::create_directories(std::filesystem::path(gen_key_path).parent_path());
            std::ofstream(gen_key_path) << kp.private_key_hex << "\n";
            std::ofstream(gen_key_path + ".pub") << kp.public_key_hex << "\n";
            return 0;
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
