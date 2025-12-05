load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
  name = "bit-client-lib",
  srcs = ["torrent_file.cc", "bdecoder.cc"],
  hdrs = ["torrent_file.h", "bdecoder.h"],
)

cc_binary(
  name = "bit-client",
  srcs = ["main.cc"],
  deps = [
    ":bit-client-lib",
  ],
)
