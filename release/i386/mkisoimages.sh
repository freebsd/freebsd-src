#!/bin/sh
#
# Module: mkisoimages.sh
# Author: Jordan K Hubbard
# Date:   22 June 2001
#
# $FreeBSD$
#
# This script is used by release/Makefile to build the (optional) ISO images
# for a FreeBSD release.  It is considered architecture dependent since each
# platform has a slightly unique way of making bootable CDs.  This script
# is also allowed to generate any number of images since that is more of
# publishing decision than anything else.
#
# Usage:
#
# mkisoimages.sh [-b] image-label image-name base-bits-dir [extra-bits-dir]
#
# Where -b is passed if the ISO image should be made "bootable" by
# whatever standards this architecture supports (may be unsupported),
# image-label is the ISO image label, image-name is the filename of the
# resulting ISO image, base-bits-dir contains the image contents and
# extra-bits-dir, if provided, contains additional files to be merged
# into base-bits-dir as part of making the image.

if [ "x$1" = "x-b" ]; then
	# This is highly x86-centric and will be used directly below.
	bootable="-b boot/cdboot -no-emul-boot"
	shift
else
	bootable=""
fi

if [ $# -lt 3 ]; then
	echo Usage: $0 '[-b] image-label image-name base-bits-dir [extra-bits-dir]'
	exit 1
fi

type mkisofs 2>&1 | grep " is " >/dev/null
if [ $? -ne 0 ]; then
	echo The mkisofs port is not installed.  Trying to get it now.
	if [ -f /usr/ports/sysutils/mkisofs/Makefile ]; then
		cd /usr/ports/sysutils/mkisofs && make install && make clean
	else
		if ! pkg_add -r mkisofs; then
			echo "Could not get it via pkg_add - please go install this"
			echo "from the ports collection and run this script again."
			exit 2
		fi
	fi
fi

LABEL=$1; shift
NAME=$1; shift

mkisofs $bootable -r -J -V $LABEL -o $NAME $*
