#include "torrent_file.h"
#include "bdecoder.h"
#include "utils.h"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <ios>
#include <iostream>
#include <stdexcept>
#include <string>

void TorrentFile::parse() {
  readFile();
  readFileBytes();
  BNode node_res = bdecode(m_contents);
  node_res.print(std::cout);

  auto info = node_res["info"];
  auto orig_info = info.encode();
  std::vector<uint8_t> orig_info_bytes(orig_info.begin(), orig_info.end());

  std::string info_hash = sha1(orig_info_bytes);
  std::cout << "Info hash= " << info_hash << "\n";
}

void TorrentFile::readFile() {
  std::ifstream file(m_file_name, std::ios::in);

  if (!file.good()) {
    throw std::runtime_error("No such file exists in this universe.");
  }

  file.unsetf(std::ios::skipws);
  file.seekg(0, std::ios::end);
  auto file_len = file.tellg();
  file.seekg(0, std::ios::beg);

  m_contents.resize(static_cast<size_t>(file_len));

  file.read(m_contents.data(), file_len);

  if (file.gcount() != file_len) {
    throw std::runtime_error("Uh oh that did not read the entire file data.");
  }

  file.close();
}

void TorrentFile::readFileBytes() {
  std::cout << "filename: " << m_file_name << "\n";
  std::ifstream file(m_file_name, std::ios::binary);

  if (!file.good()) {
    throw std::runtime_error("No such file exists in this universe.");
  }

  file.unsetf(std::ios::skipws);
  file.seekg(0, std::ios::end);
  auto file_len = file.tellg();
  file.seekg(0, std::ios::beg);

  m_contents_bytes.resize(static_cast<size_t>(file_len));

  file.read(reinterpret_cast<char *>(m_contents_bytes.data()), file_len);

  if (file.gcount() != file_len) {
    throw std::runtime_error("Uh oh that did not read the entire file data.");
  }

  file.close();
}
