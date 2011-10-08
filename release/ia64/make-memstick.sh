#!/bin/sh
#
# This script generates a "memstick image" (image that can be copied to a
# USB memory stick) from a directory tree.  Note that the script does not
# clean up after itself very well for error conditions on purpose so the
# problem can be diagnosed (full filesystem most likely but ...).
#
# Usage: make-memstick.sh <directory tree> <image filename>
#
# $FreeBSD$
#

PATH=/bin:/usr/bin:/sbin:/usr/sbin
export PATH

BLOCKSIZE=10240

if [ $# -ne 2 ]; then
  echo "make-memstick.sh /path/to/directory /path/to/image/file"
  exit 1
fi

if [ ! -d ${1} ]; then
  echo "${1} must be a directory"
  exit 1
fi

if [ -e ${2} ]; then
  echo "won't overwrite ${2}"
  exit 1
fi

makefs -B little ${2} ${1}
if [ $? -ne 0 ]; then
  echo "makefs failed"
  exit 1
fi

