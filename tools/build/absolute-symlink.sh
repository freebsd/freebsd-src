#!/bin/sh
# Copyright (c) Oct 2024 Wolfram Schneider <wosch@FreeBSD.org>
# SPDX-License-Identifier: BSD-2-Clause
#
# absolute-symlink.sh - check for absolute symlinks on a FreeBSD system
#
# The purpose of this script is to detect absolute symlinks on
# a machine, e.g.: 
#
#    /etc/localtime -> /usr/share/zoneinfo/UTC
#
# Some of these absolute symbolic links can be created intentionally,
# but it is usually better to use relative symlinks.
#
# You can run the script after `make installworld', or any other
# make targets thats installs files.
#
# You can also check your local ports with:
#    
#   env ABSOLUTE_SYMLINK_DIRS=/usr/local ./absolute-symlink.sh


PATH="/bin:/usr/bin"; export PATH
LANG="C"; export LANG

# check other directories as well
: ${ABSOLUTE_SYMLINK_DIRS=""}

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
  $ABSOLUTE_SYMLINK_DIRS \
  -type l \
  -ls | grep -Ea -- ' -> /'

#EOF
