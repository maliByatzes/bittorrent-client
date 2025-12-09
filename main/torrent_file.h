#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

class TorrentFile {
private:
  std::string m_file_name;
  std::string m_contents;
  std::vector<uint8_t> m_contents_bytes;

  void readFile();
  void readFileBytes();

public:
  TorrentFile(const std::string file_name)
      : m_file_name(std::move(file_name)) {}

  void parse();
};
