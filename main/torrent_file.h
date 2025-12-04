#pragma once

#include <string>
#include <utility>

class TorrentFile {
private:
  std::string file_name_;
  std::string contents_;

  void readFile();

public:
  TorrentFile(const std::string &file_name)
      : file_name_(std::move(file_name)) {}

  void parse();
};
