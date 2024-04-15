#!/bin/bash

set -x

# Cause the script to exit if a single command fails.
set -e

TP_DIR=$(cd "$(dirname "${BASH_SOURCE:-$0}")"; pwd)

FLATBUFFERS_VERSION=1.7.1

# Download and compile flatbuffers if it isn't already present.
if [ ! -d $TP_DIR/flatbuffers ]; then
  echo "building flatbuffers"
  wget https://github.com/google/flatbuffers/archive/v$FLATBUFFERS_VERSION.tar.gz -O flatbuffers-$FLATBUFFERS_VERSION.tar.gz
  tar xf flatbuffers-$FLATBUFFERS_VERSION.tar.gz
  rm -rf flatbuffers-$FLATBUFFERS_VERSION.tar.gz

  # Compile flatbuffers.
  pushd flatbuffers-$FLATBUFFERS_VERSION
    cmake -DCMAKE_CXX_FLAGS=-fPIC \
          -DCMAKE_INSTALL_PREFIX:PATH=$TP_DIR/flatbuffers \
          -DFLATBUFFERS_BUILD_TESTS=OFF
    make -j5
    make install
  popd
fi
