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

tempfile="${2}.$$"

if [ ! -d ${1} ]; then
  echo "${1} must be a directory"
  exit 1
fi

if [ -e ${2} ]; then
  echo "won't overwrite ${2}"
  exit 1
fi

rm -f ${tempfile}
makefs ${tempfile} ${1}
if [ $? -ne 0 ]; then
  echo "makefs failed"
  exit 1
fi

#
# Use $BLOCKSIZE for transfers to improve efficiency.  When calculating
# how many blocks to transfer "+ 2" is to account for truncation in the
# division and to provide space for the label.
#

filesize=`stat -f "%z" ${tempfile}`
blocks=$(($filesize / ${BLOCKSIZE} + 2))
dd if=/dev/zero of=${2} bs=${BLOCKSIZE} count=${blocks}
if [ $? -ne 0 ]; then
  echo "creation of image file failed"
  exit 1
fi

unit=`mdconfig -a -t vnode -f ${2}`
if [ $? -ne 0 ]; then
  echo "mdconfig failed"
  exit 1
fi

fdisk -BIq /dev/${unit}
if [ $? -ne 0 ]; then
  echo "fdisk failed"
  exit 1
fi

bsdlabel -B -w /dev/${unit}
if [ $? -ne 0 ]; then
  echo "bsdlabel failed"
  exit 1
fi

dd if=${tempfile} of=/dev/${unit}a bs=$BLOCKSIZE conv=sync
if [ $? -ne 0 ]; then
  echo "copying filesystem into image file failed"
  exit 1
fi

mdconfig -d -u ${unit}

rm -f ${tempfile}

