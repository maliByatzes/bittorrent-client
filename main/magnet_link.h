#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

struct MagnetLink {
  std::array<uint8_t, 20> info_hash;
  std::string info_hash_hex;
  std::string display_name;
  std::vector<std::string> tracker_urls;

  uint64_t exact_length;
  bool has_exact_length;

  MagnetLink() : exact_length(0), has_exact_length(false) {}

  bool isValid() const;
};

class MagnetParser {
public:
  static MagnetLink parse(const std::string &magnet_uri);

private:
  static std::string urlDecode(const std::string &str);
  static bool parseInfoHash(const std::string &xt_value,
                            std::array<uint8_t, 20> &info_hash);
  static bool hexToBytes(const std::string &hex,
                          std::array<uint8_t, 20> &bytes);
};
