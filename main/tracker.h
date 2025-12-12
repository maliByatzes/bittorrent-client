#pragma once

#include "bdecoder.h"
#include "torrent_file.h"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

struct PeerInfo {
  std::string ip;
  uint16_t port;
  std::string peer_id;

  PeerInfo(const std::string &ip_addr, uint16_t p) : ip(ip_addr), port(p) {}

  PeerInfo(const std::string &ip_addr, uint16_t p, const std::string &id)
      : ip(ip_addr), port(p), peer_id(id) {}
};

struct TrackerResponse {
  bool success;
  std::string failure_reason;

  int interval;
  int complete;
  int incomplete;
  std::vector<PeerInfo> peers;

  TrackerResponse() : success(false), interval(0), complete(0), incomplete(0) {}
};

class Tracker {
private:
  std::string m_announce_url;
  std::array<uint8_t, 20> m_info_hash;
  std::string m_peer_id;
  uint16_t m_port;

  uint64_t m_uploaded;
  uint64_t m_downloaded;
  uint64_t m_left;

public:
  Tracker(const std::string &announce_url,
          const std::array<uint8_t, 20> &info_hash, const std::string &peer_id,
          uint16_t port, uint64_t total_size);

  TrackerResponse announce(const std::string &event = "");
  void updateStats(uint64_t uploaded, uint64_t downloaded, uint64_t left);
  int getInterval() const { return m_last_interval; }

private:
  int m_last_interval;

  std::string buildAnnounceUrl(const std::string &event) const;
  TrackerResponse parseTrackerResponse(const std::string &response_body) const;
  std::vector<PeerInfo> parseCompactPeers(const std::string &peers_data) const;
  std::vector<PeerInfo> parseDictionaryPeers(const BNode &peers_list) const;
  static std::string urlEncode(const uint8_t *data, size_t length);
};
