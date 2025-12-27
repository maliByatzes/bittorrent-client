load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
  name = "utils",
  srcs = ["utils.cc"],
  hdrs = ["utils.h"],
)

cc_library(
  name = "bdecoder",
  srcs = ["bdecoder.cc"],
  hdrs = ["bdecoder.h"],
)

cc_library(
  name = "http_client",
  srcs = ["http_client.cc"],
  hdrs = ["http_client.h"],
)

cc_library(
  name = "magnet_link",
  srcs = ["magnet_link.cc"],
  hdrs = ["magnet_link.h"],
)

cc_library(
  name = "torrent_file",
  srcs = ["torrent_file.cc"],
  hdrs = ["torrent_file.h"],
  deps = [
    ":bdecoder",
    ":utils",
  ],
)

cc_library(
  name = "tracker",
  srcs = ["tracker.cc"],
  hdrs = ["tracker.h"],
  deps = [
    ":bdecoder",
    ":http_client",
    ":torrent_file",
    ":utils",
  ],
)

cc_library(
  name = "peer_connection",
  srcs = ["peer_connection.cc"],
  hdrs = ["peer_connection.h"],
  deps = [
    ":torrent_file"
  ],
)

cc_library(
  name = "metadata_fetcher",
  srcs = ["metadata_fetcher.cc"],
  hdrs = ["metadata_fetcher.h"],
  deps = [
    ":peer_connection",
    ":torrent_file",
    ":magnet_link",
    ":bdecoder"
  ],
)

cc_library(
  name = "resume_state",
  srcs = ["resume_state.cc"],
  hdrs = ["resume_state.h"],
)

cc_library(
  name = "upload_manager",
  srcs = ["upload_manager.cc"],
  hdrs = ["upload_manager.h"],
  deps = [
    ":peer_connection",
    ":torrent_file"
  ],
)

cc_library(
  name = "download_manager",
  srcs = ["download_manager.cc"],
  hdrs = ["download_manager.h"],
  deps = [
    ":peer_connection",
    ":torrent_file",
    ":utils",
    ":resume_state",
    ":upload_manager"
  ],
)

cc_binary(
  name = "bittorrent_client",
  srcs = ["main.cc"],
  deps = [
    ":torrent_file",
    ":tracker",
    ":utils",
    ":peer_connection",
    ":download_manager",
    ":magnet_link",
    ":metadata_fetcher"
  ],
)
