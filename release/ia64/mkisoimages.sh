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

set -e

# The hackery function is to help with the development of the release
# process. It's not intended to be an integral part of it. JFYI...
hackery() {
    echo "Sorry, no hackery today and you're out of milk too"
    exit 2
}

MKISOFS_PORT=/usr/ports/sysutils/mkisofs

if [ "x$1" = "x-b" ]; then
    bootable=yes
    shift
else
    bootable=no
fi

if [ $# -lt 3 ]; then
    echo usage: $0 '[-b] label iso-name base-dir [extra-dir]'
    exit 1
fi

BOOTOPTS=""
LABEL=$1; shift
NAME=$1; shift
BASE=$1; shift

if ! which mkisofs; then
    echo 'mkisofs(8) does not exist. Fetching the package...'
    if ! pkg_add -r mkisofs; then
	if [ -f /usr/ports/sysutils/mkisofs/Makefile ]; then
	    echo "Don't worry; building the port..."
	    if ! (cd $MKISOFS_PORT && make install && make clean); then
		echo "Worry; reverting to hackery..."
		hackery
	    fi
	else
	    echo "Ports not present. Reverting to hackery..."
	    hackery
	fi
    fi
fi

EFIPART=efipart.sys

# To create a bootable CD under EFI, the boot image should be an EFI
# system partition. Since we already made that on the boot floppy,
# we sneakily extract that.
if [ $bootable = yes ]; then
    EFISZ=20480
    MNT=/mnt
    dd if=/dev/zero of=$BASE/$EFIPART count=$EFISZ
    md=`mdconfig -a -t vnode -f $BASE/$EFIPART`
    newfs_msdos -F 12 -S 512 -h 4 -o 0 -s $EFISZ -u 16 $md
    mount -t msdos /dev/$md $MNT
    mkdir -p $MNT/efi/boot $MNT/boot $MNT/boot/kernel
    cp -R $BASE/boot/defaults $MNT/boot
    cp $BASE/boot/kernel/kernel $MNT/boot/kernel
    cp $BASE/boot/device.hints $MNT/boot
    cp $BASE/boot/loader.* $MNT/boot
    cp $BASE/boot/support.4th $MNT/boot
    mv $MNT/boot/loader.efi $MNT/efi/boot/bootia64.efi
    umount $MNT
    mdconfig -d -u $md
    BOOTOPTS="-b $EFIPART -no-emul-boot"
fi

mkisofs $BOOTOPTS -r -J -V $LABEL -o $NAME $BASE $*
rm -f $BASE/$EFIPART
exit 0
