#include "tracker.h"
#include "bdecoder.h"
#include "http_client.h"
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

// TrackerResponse announce(const std::string &event = "");
// int getInterval() const { return m_last_interval; }

// TrackerResponse parseTrackerResponse(const std::string &response_body) const;
// std::vector<PeerInfo> parseCompactPeers(const std::string &peers_data) const;
// std::vector<PeerInfo> parseDictionaryPeers(const BNode &peers_list) const;
