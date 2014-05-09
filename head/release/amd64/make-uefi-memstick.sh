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

echo '/dev/ufs/FreeBSD_Install / ufs ro,noatime 1 1' > ${1}/etc/fstab
makefs -B little -o label=FreeBSD_Install ${2}.part ${1}
if [ $? -ne 0 ]; then
	echo "makefs failed"
	exit 1
fi
rm ${1}/etc/fstab

mkimg -s gpt -b ${1}/boot/pmbr -p efi:=${1}/boot/boot1.efifat -p freebsd-boot:=${1}/boot/gptboot -p freebsd-ufs:=${2}.part -p freebsd-swap::1M -o ${2}
rm ${2}.part

