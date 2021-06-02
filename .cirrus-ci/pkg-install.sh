#!/bin/sh
set -e

pkg install -y "$@" && exit 0

cat <<EOF
pkg install failed

dmesg tail:
$(dmesg | tail)

trying again
EOF

pkg install -y "$@"
