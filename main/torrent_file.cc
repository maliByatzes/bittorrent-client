#include "torrent_file.h"
#include <cstddef>
#include <fstream>
#include <ios>
#include <iostream>

void TorrentFile::parse() {
  readFile();

  std::cout << "File contents:\n";
  std::cout << contents_;
}

void TorrentFile::readFile() {
  std::fstream file(file_name_, std::ios::in);

  if (!file.good()) {
    throw "No such file exists in this universe.";
  }

  file.unsetf(std::ios::skipws);
  file.seekg(0, std::ios::end);
  auto file_len = file.tellg();
  file.seekg(0, std::ios::beg);

  contents_.resize(static_cast<size_t>(file_len));

  file.read(contents_.data(), file_len);
  file.close();
}
