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
	# Apple boot code
	uudecode -o /tmp/hfs-boot-block.bz2 `dirname $0`/hfs-boot.bz2.uu
	bzip2 -d /tmp/hfs-boot-block.bz2
	OFFSET=$(hd /tmp/hfs-boot-block | grep 'Loader START' | cut -f 1 -d ' ')
	OFFSET=0x$(echo 0x$OFFSET | awk '{printf("%x\n",$1/512);}')
	dd if=$4/boot/loader of=/tmp/hfs-boot-block seek=$OFFSET conv=notrunc

	bootable="-o bootimage=macppc;/tmp/hfs-boot-block -o no-emul-boot"

	# pSeries/PAPR boot code
	mkdir $4/ppc
	cat > $4/ppc/bootinfo.txt << EOF
<CHRP-BOOT>
<DESCRIPTION>FreeBSD/powerpc</DESCRIPTION>
<OS-NAME>FreeBSD</OS-NAME>
<BOOT-SCRIPT>
boot &device;:&partition;,\boot\loader &device;:0
</BOOT-SCRIPT>
</CHRP-BOOT>
EOF

	shift
else
	bootable=""
fi

if [ $# -lt 3 ]; then
	echo Usage: $0 '[-b] image-label image-name base-bits-dir [extra-bits-dir]'
	exit 1
fi

LABEL=$1; shift
NAME=$1; shift

echo "/dev/iso9660/`echo $LABEL | tr '[:lower:]' '[:upper:]'` / cd9660 ro 0 0" > $1/etc/fstab
makefs -t cd9660 $bootable -o rockridge -o label=$LABEL $NAME $*
rm $1/etc/fstab
rm /tmp/hfs-boot-block
rm -rf $1/ppc

