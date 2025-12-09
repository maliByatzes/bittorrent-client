#include "torrent_file.h"
#include "bdecoder.h"
#include <cstddef>
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
  std::string announce = node_res["announce"].asString();
  auto info = node_res["info"];
  auto name = info["name"].asString();
  auto length = info["length"].asInteger();
  auto piece_length = info["piece length"].asInteger();
  auto pieces = info["pieces"].asString();

  std::cout << "announce= " << announce << "\n\n";
  std::cout << "info=\n";
  std::cout << "  name= " << name << "\n";
  std::cout << "  length= " << length << "\n";
  std::cout << "  piece length= " << piece_length << "\n";

  std::cout << "Data in bytes:\n";
  for (const auto &byte : m_contents_bytes) {
    std::cout << unsigned(byte) << "  ";
  }
  std::cout << "\n";
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
