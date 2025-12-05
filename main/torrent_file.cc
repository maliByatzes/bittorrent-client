#include "torrent_file.h"
#include "bdecoder.h"
#include <cstddef>
#include <fstream>
#include <ios>
#include <iostream>
#include <string>

void TorrentFile::parse() {
  readFile();
  BNode node_res = bdecode(m_contents);
  std::cout << "Contents: ";
  node_res.print(std::cout);
  std::cout << "\n\n";
}

void TorrentFile::readFile() {
  std::fstream file(m_file_name, std::ios::in);

  if (!file.good()) {
    throw "No such file exists in this universe.";
  }

  file.unsetf(std::ios::skipws);
  file.seekg(0, std::ios::end);
  auto file_len = file.tellg();
  file.seekg(0, std::ios::beg);

  m_contents.resize(static_cast<size_t>(file_len));

  file.read(m_contents.data(), file_len);
  file.close();
}
