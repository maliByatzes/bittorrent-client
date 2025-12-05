#pragma once

#include <string>
#include <utility>

class TorrentFile {
private:
  std::string m_file_name;
  std::string m_contents;

  void readFile();

public:
  TorrentFile(const std::string file_name)
      : m_file_name(std::move(file_name)) {}

  void parse();
};
