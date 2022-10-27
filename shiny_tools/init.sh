#!/bin/bash
source shiny_tools/source.sh
echo "Building bmake..." &&
./tools/build/make.py TARGET=arm64 TARGET_ARCH=aarch64 --cross-bindir=$CROSSBIN --debug -n -j16 &
echo "Building toolchain..." &&
./tools/build/make.py TARGET=arm64 TARGET_ARCH=aarch64 --cross-bindir=$CROSSBIN --debug -j16 kernel-toolchain &&
echo "Done"
