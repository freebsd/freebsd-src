#!/bin/sh
# Copyright (c) Feb 2024 Wolfram Schneider <wosch@FreeBSD.org>
# SPDX-License-Identifier: BSD-2-Clause
#
# stale-symlink-buildworld.sh - check for stale symlinks on a FreeBSD system
#
# The purpose of this script is to detect stale symlinks, report them to
# stderr and exit with a non-zero status. All other cases are ignored,
# such as no symlinks, missing directories, permission problems, etc.
#
# You can run the script before or after `make installworld', or any other
# make targets thats installs files.
#
# You can also check your local ports with:
#    
#   env STALE_SYMLINK_BUILDWORLD_DIRS=/usr/local ./stale-symlink-buildworld.sh


PATH="/bin:/usr/bin"; export PATH

: ${ncpu=$(nproc)}

obj_dir_prefix=${MAKEOBJDIRPREFIX:="/usr/obj"}

# check other directories as well
: ${STALE_SYMLINK_BUILDWORLD_DIRS=$obj_dir_prefix}

trap 'rm -f $script' 0
script=$(mktemp -t stale-symlink)
chmod u+x $script

# create a temporary shell script to check for stale symbolic links
cat << 'EOF' > $script
file="$1"

if [ ! -e "$file" ]; then
  echo "stale symlink detected: $(ls -ld $file)" >&2
  exit 1
else
  exit 0
fi
EOF

find -s -H \
  /bin \
  /boot \
  /etc \
  /lib \
  /libexec \
  /sbin \
  /usr/bin \
  /usr/include \
  /usr/lib \
  /usr/lib32 \
  /usr/libdata \
  /usr/libexec \
  /usr/sbin \
  /usr/src \
  /usr/share \
  $STALE_SYMLINK_BUILDWORLD_DIRS \
  -type l \
  -print0 | xargs -n1 -0 -P${ncpu} $script

#EOF
