#include "magnet_link.h"
#include <sstream>
#include <algorithm>
#include <stdexcept>

bool MagnetLink::isValid() const {
    bool has_hash = false;
    for (uint8_t byte : info_hash) {
        if (byte != 0) {
            has_hash = true;
            break;
        }
    }

    return has_hash;
}

std::string MagnetParser::urlDecode(const std::string &str) {
    std::string result;

    for (size_t i = 0; i < str.length(); i++) {
        if (str[i] == '%' && i + 2 < str.length()) {
            std::string hex = str.substr(i + 1, 2);
            int value = std::stoi(hex, nullptr, 16);
            result += static_cast<char>(value);
            i += 2;
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }

    return result;
}

bool MagnetParser::hexToBytes(const std::string &hex, std::array<uint8_t, 20> &bytes) {
    if (hex.length() != 40) { return false; }
    
    for (size_t i = 0; i < 20; i++) {
        std::string byte_str = hex.substr(i * 2, 2);
        try {
            bytes[i] = static_cast<uint8_t>(std::stoi(byte_str, nullptr, 16));
        } catch (...) {
            return  false;
        }
    }

    return true;
}

bool MagnetParser::parseInfoHash(const std::string &xt_value, std::array<uint8_t, 20> &info_hash) {
    size_t hash_start = xt_value.find("btih:");
    if (hash_start == std::string::npos) { return false; }

    hash_start += 5;
    std::string hash_str = xt_value.substr(hash_start);

    size_t param_pos = hash_str.find('&');
    if (param_pos != std::string::npos) {
        hash_str = hash_str.substr(0, param_pos);
    }

    std::transform(hash_str.begin(), hash_str.end(), hash_str.begin(), ::tolower);

    return hexToBytes(hash_str, info_hash);
}

MagnetLink MagnetParser::parse(const std::string &magnet_uri) {
    MagnetLink magnet;

    if (magnet_uri.substr(0, 8) != "magnet:?") {
        throw std::runtime_error("Invalid magnet URI: must start with 'magnet:?'");
    }

    std::string query = magnet_uri.substr(8);

    std::istringstream query_param(query);
    std::string param;

    while (std::getline(query_param, param, '&')) {
        size_t eq_pos = param.find('=');
        if (eq_pos == std::string::npos) {
            continue;
        }

        std::string key = param.substr(0, eq_pos);
        std::string value = param.substr(eq_pos + 1);

        if (key == "xt") {
            if (!parseInfoHash(value, magnet.info_hash)) {
                throw std::runtime_error("Failed to parse info hash from magnet link");
            }

            magnet.info_hash_hex = "";
            for (uint8_t byte : magnet.info_hash) {
                char hex[3];
                snprintf(hex, sizeof(hex), "%02X", byte);
                magnet.info_hash_hex += hex;
            }
        } else if (key == "dn") {
            magnet.display_name = urlDecode(value);
        } else if (key == "tr") {
            magnet.tracker_urls.push_back(urlDecode(value));
        } else if (key == "xl") {
            try {
                magnet.exact_length = std::stoull(value);
                magnet.has_exact_length = true;
            } catch (...) {}
        }
    }

    if (magnet.isValid()) {
        throw std::runtime_error("Invalid magnet link: missing info hash");
    }

    return magnet;
}