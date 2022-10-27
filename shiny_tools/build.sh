#!/bin/zsh
source shiny_tools/source.sh

echo "Building kernel..." &&
./tools/build/make.py TARGET=arm64 TARGET_ARCH=aarch64 --cross-bindir=$CROSSBIN -DNO_CLEAN --debug -j16 buildkernel &&
echo "Done"
