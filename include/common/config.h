#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace rbft {

struct PeerConfig {
    std::string node_id;
    std::string host;
    int rest_port{};
    int p2p_port{};
    std::string public_key;
};

struct NodeConfig {
    std::string node_id;
    int rest_port{};
    int p2p_port{};
    std::string db_path;
    std::string node_private_key_path;
    std::string node_public_key;
    std::string chain_id;
    int f{1};
    int instance_count{2};
    int block_interval_ms{1500};
    int consensus_timeout_ms{5000};
    std::vector<PeerConfig> peers;
};

NodeConfig LoadConfig(const std::string& path);

} // namespace rbft
