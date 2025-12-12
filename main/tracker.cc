#include "tracker.h"
#include "bdecoder.h"
#include "http_client.h"
#include <cstdint>
#include <iomanip>
#include <ios>
#include <sstream>
#include <stdexcept>

Tracker::Tracker(const std::string &announce_url,
                 const std::array<uint8_t, 20> &info_hash,
                 const std::string &peer_id, uint16_t port, uint64_t total_size)
    : m_announce_url(announce_url), m_info_hash(info_hash), m_peer_id(peer_id),
      m_port(port), m_uploaded(0), m_downloaded(0), m_left(total_size),
      m_last_interval(1800) {
  if (peer_id.length() != 20) {
    throw std::runtime_error("Peer ID must exactly 20 bytes");
  }
}

void Tracker::updateStats(uint64_t uploaded, uint64_t downloaded,
                          uint64_t left) {
  m_uploaded = uploaded;
  m_downloaded = downloaded;
  m_left = left;
}

std::string urlEncode(const uint8_t *data, size_t length) {
  std::ostringstream encoded;
  encoded << std::hex << std::uppercase << std::setfill('0');

  for (size_t i = 0; i < length; i++) {
    encoded << '%' << std::setw(2) << static_cast<int>(data[i]);
  }

  return encoded.str();
}

std::string Tracker::buildAnnounceUrl(const std::string &event) const {
  std::ostringstream url;

  url << m_announce_url;

  if (m_announce_url.find('?') == std::string::npos) {
    url << '?';
  } else {
    url << '&';
  }

  url << "info_hash=" << urlEncode(m_info_hash.data(), m_info_hash.size());
  url << "&peer_id="
      << urlEncode(reinterpret_cast<const uint8_t *>(m_peer_id.data()),
                   m_peer_id.length());
  url << "&port=" << m_port;
  url << "&uploaded=" << m_uploaded;
  url << "&downloaded=" << m_downloaded;
  url << "&left=" << m_left;
  url << "&compact=1";

  if (!event.empty()) {
    url << "&event=" << event;
  }

  return url.str();
}

std::vector<PeerInfo>
Tracker::parseCompactPeers(const std::string &peers_data) const {
  std::vector<PeerInfo> peers;

  if (peers_data.length() % 6 != 0) {
    throw std::runtime_error("Invalid compact peers data length");
  }

  size_t num_peers = peers_data.length() / 6;

  for (size_t i = 0; i < num_peers; i++) {
    size_t offset = i * 6;

    uint8_t ip_bytes[4];
    for (int j = 0; j < 4; j++) {
      ip_bytes[j] = static_cast<uint8_t>(peers_data[offset + j]);
    }

    std::ostringstream ip_stream;
    ip_stream << static_cast<int>(ip_bytes[0]) << '.'
              << static_cast<int>(ip_bytes[1]) << '.'
              << static_cast<int>(ip_bytes[2]) << '.'
              << static_cast<int>(ip_bytes[3]);

    uint16_t port = (static_cast<uint8_t>(peers_data[offset + 4]) << 8) |
                    static_cast<uint8_t>(peers_data[offset + 5]);

    peers.emplace_back(ip_stream.str(), port);
  }

  return peers;
}

std::vector<PeerInfo>
Tracker::parseDictionaryPeers(const BNode &peers_list) const {
  std::vector<PeerInfo> peers;

  if (!peers_list.isList()) {
    throw std::runtime_error("Expected peers to be a list");
  }

  for (const auto &peer_node : peers_list.asList()) {
    if (!peer_node.isDictionary()) {
      continue;
    }

    try {
      std::string ip = peer_node["ip"].asString();
      int64_t port_int = peer_node["port"].asInteger();

      if (port_int < 0 || port_int > 65535) {
        continue;
      }

      uint16_t port = static_cast<uint16_t>(port_int);

      std::string peer_id;
      if (peer_node.isDictionary() && peer_node.asDict().count("peer id")) {
        peer_id = peer_node["peer id"].asString();
      }

      peers.emplace_back(ip, port, peer_id);
    } catch (const std::exception &e) {
      continue;
    }
  }

  return peers;
}

// TrackerResponse announce(const std::string &event = "");
// int getInterval() const { return m_last_interval; }

// TrackerResponse parseTrackerResponse(const std::string &response_body) const;
