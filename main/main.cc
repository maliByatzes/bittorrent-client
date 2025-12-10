#include "torrent_file.h"
#include <string>

int main() {
  std::string file_name{"sample.torrent"};

  TorrentFile torrent_file(file_name);
  torrent_file.parse();
}
