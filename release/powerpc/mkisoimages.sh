#!/bin/sh
#
# Module: mkisoimages.sh
# Author: Jordan K Hubbard
# Date:   22 June 2001
#
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

set -e

if [ "$1" = "-b" ]; then
	MAKEFSARG="$4"
else
	MAKEFSARG="$3"
fi

if [ -f ${MAKEFSARG} ]; then
	BASEBITSDIR=`dirname ${MAKEFSARG}`
	METALOG=${MAKEFSARG}
elif [ -d ${MAKEFSARG} ]; then
	BASEBITSDIR=${MAKEFSARG}
	METALOG=
else
	echo "${MAKEFSARG} must exist"
	exit 1
fi

if [ "$1" = "-b" ]; then
	bootable=1
	shift
else
	bootable=""
fi

if [ $# -lt 3 ]; then
	echo "Usage: $0 [-b] image-label image-name base-bits-dir [extra-bits-dir]"
	exit 1
fi

LABEL=`echo "$1" | tr '[:lower:]' '[:upper:]'`; shift
NAME="$1"; shift
# MAKEFSARG extracted already
shift

if [ -n "${METALOG}" ]; then
	metalogfilename=$(mktemp /tmp/metalog.XXXXXX)
	cat ${METALOG} > ${metalogfilename}
	MAKEFSARG=${metalogfilename}
fi

if [ -n "$bootable" ]; then
	echo "Building bootable disc"

	# Apple boot code
	uudecode -o /tmp/hfs-boot-block.bz2 "`dirname "$0"`/hfs-boot.bz2.uu"
	bzip2 -d /tmp/hfs-boot-block.bz2
	OFFSET=$(hd /tmp/hfs-boot-block | grep 'Loader START' | cut -f 1 -d ' ')
	OFFSET=0x$(echo 0x$OFFSET | awk '{printf("%x\n",$1/512);}')
	dd if="$BASEBITSDIR/boot/loader" of=/tmp/hfs-boot-block seek=$OFFSET conv=notrunc

	bootable="-o bootimage=macppc;/tmp/hfs-boot-block -o no-emul-boot"

	# pSeries/PAPR boot code
	mkdir -p "$BASEBITSDIR/ppc/chrp"
	cp "$BASEBITSDIR/boot/loader" "$BASEBITSDIR/ppc/chrp"
	cat > "$BASEBITSDIR/ppc/bootinfo.txt" << EOF
<chrp-boot>
<description>FreeBSD Install</description>
<os-name>FreeBSD</os-name>
<boot-script>boot &device;:,\ppc\chrp\loader</boot-script>
</chrp-boot>
EOF
	bootable="$bootable -o chrp-boot"
	if [ -n "${METALOG}" ]; then
		echo "./ppc type=dir uname=root gname=wheel mode=0755" >> ${metalogfilename}
		echo "./ppc/chrp type=dir uname=root gname=wheel mode=0755" >> ${metalogfilename}
		echo "./ppc/chrp/loader type=file uname=root gname=wheel mode=0644" >> ${metalogfilename}
		echo "./ppc/bootinfo.txt type=file uname=root gname=wheel mode=0644" >> ${metalogfilename}
	fi

	# Petitboot config for PS3/PowerNV
	echo FreeBSD Install=\'/boot/kernel/kernel vfs.root.mountfrom=cd9660:/dev/iso9660/$LABEL\' > "$BASEBITSDIR/etc/kboot.conf"
	if [ -n "${METALOG}" ]; then
		echo "./etc/kboot.conf type=file uname=root gname=wheel mode=0644" >> ${metalogfilename}
	fi
fi

publisher="The FreeBSD Project.  https://www.FreeBSD.org/"
echo "/dev/iso9660/$LABEL / cd9660 ro 0 0" > "$BASEBITSDIR/etc/fstab"
if [ -n "${METALOG}" ]; then
	echo "./etc/fstab type=file uname=root gname=wheel mode=0644" >> ${metalogfilename}
fi
makefs -D -N ${BASEBITSDIR}/etc -t cd9660 $bootable -o rockridge -o label="$LABEL" -o publisher="$publisher" "$NAME" "$MAKEFSARG" "$@"
rm -f "$BASEBITSDIR/etc/fstab"
rm -f /tmp/hfs-boot-block
rm -rf "$BASEBITSDIR/ppc"
if [ -n "${METALOG}" ]; then
	rm ${metalogfilename}
fi
