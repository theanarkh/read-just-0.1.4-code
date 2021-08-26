#!/bin/bash
MODULES=${2:-"blake3 encode epoll ffi fs html http inspector memory net openssl pg seccomp sha1 signal sys tcc thread udp vm zlib profiler rocksdb"}
for m in $MODULES
do
  make -C $m/ clean library
  sudo make -C $m/ install
  sudo make -C $m/ install-debug
done
