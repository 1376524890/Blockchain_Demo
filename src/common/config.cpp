#include "common/config.h"

#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace rbft {

NodeConfig LoadConfig(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("failed to open config: " + path);
    }
    nlohmann::json j;
    in >> j;

    NodeConfig cfg;
    cfg.node_id = j.at("node_id").get<std::string>();
    cfg.rest_port = j.at("rest_port").get<int>();
    cfg.p2p_port = j.at("p2p_port").get<int>();
    cfg.db_path = j.at("db_path").get<std::string>();
    cfg.node_private_key_path = j.at("node_private_key_path").get<std::string>();
    cfg.node_public_key = j.value("node_public_key", "");
    cfg.chain_id = j.at("chain_id").get<std::string>();
    cfg.f = j.value("f", 1);
    cfg.instance_count = j.value("instance_count", 2);
    cfg.block_interval_ms = j.value("block_interval_ms", 1500);
    cfg.consensus_timeout_ms = j.value("consensus_timeout_ms", 5000);
    for (const auto& p : j.at("peers")) {
        cfg.peers.push_back(PeerConfig{
            p.at("node_id").get<std::string>(),
            p.value("host", "127.0.0.1"),
            p.at("rest_port").get<int>(),
            p.at("p2p_port").get<int>(),
            p.value("public_key", "")
        });
    }
    return cfg;
}

} // namespace rbft
