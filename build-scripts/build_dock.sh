#!/bin/sh
# Build script for papagan-dock (qmake/CMake depending on environment)
set -e

# Allow overriding PREFIX
PREFIX=${PREFIX:-/usr/local}

echo "Building papagan-dock..."
cd "$(dirname "$0")/.."

if command -v qmake >/dev/null 2>&1; then
  qmake src/papagan-dock/papagan-dock.pro -r
  make -C src/papagan-dock -j$(nproc || echo 2)
  mkdir -p ${PREFIX}/bin
  cp src/papagan-dock/papagan-dock ${PREFIX}/bin/
  echo "Installed to ${PREFIX}/bin/papagan-dock"
else
  echo "qmake not found. Please install Qt5 development packages (pkg install qt5)."
  exit 1
fi
