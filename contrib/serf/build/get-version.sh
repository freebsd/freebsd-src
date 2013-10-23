#!/bin/sh
#
# extract version numbers from a header file
#
# USAGE: get-version.sh CMD VERSION_HEADER PREFIX
#   where CMD is one of: all, major, libtool
#   where PREFIX is the prefix to {MAJOR|MINOR|PATCH}_VERSION defines
#
#   get-version.sh all returns a dotted version number
#   get-version.sh major returns just the major version number
#   get-version.sh libtool returns a version "libtool -version-info" format
#

if test $# != 3; then
  echo "USAGE: $0 CMD VERSION_HEADER PREFIX"
  echo "  where CMD is one of: all, major, libtool"
  exit 1
fi

major_sed="/#define.*$3_MAJOR_VERSION/s/^[^0-9]*\([0-9]*\).*$/\1/p"
minor_sed="/#define.*$3_MINOR_VERSION/s/^[^0-9]*\([0-9]*\).*$/\1/p"
patch_sed="/#define.*$3_PATCH_VERSION/s/^[^0-9]*\([0-9]*\).*$/\1/p"
major="`sed -n $major_sed $2`"
minor="`sed -n $minor_sed $2`"
patch="`sed -n $patch_sed $2`"

if test "$1" = "all"; then
  echo ${major}.${minor}.${patch}
elif test "$1" = "major"; then
  echo ${major}
elif test "$1" = "libtool"; then
  # Yes, ${minor}:${patch}:${minor} is correct due to libtool idiocy.
  echo ${minor}:${patch}:${minor}
else
  echo "ERROR: unknown version CMD ($1)"
  exit 1
fi
