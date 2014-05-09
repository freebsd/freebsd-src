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

LABEL=`echo $1 | tr '[:lower:]' '[:upper:]'`; shift
NAME=$1; shift
BASE=$1; shift

EFIPART=efipart.sys

# To create a bootable CD under EFI, the boot image should be an EFI
# system partition.
if [ $bootable = yes ]; then
    EFISZ=65536
    MNT=/mnt
    dd if=/dev/zero of=$EFIPART count=$EFISZ
    md=`mdconfig -a -t vnode -f $EFIPART`
    newfs_msdos -F 12 -S 512 -h 4 -o 0 -s $EFISZ -u 16 $md
    mount -t msdosfs /dev/$md $MNT
    mkdir -p $MNT/efi/boot $MNT/boot $MNT/boot/kernel
    cp -R $BASE/boot/defaults $MNT/boot
    cp $BASE/boot/kernel/kernel $MNT/boot/kernel
    if [ -s $BASE/boot/kernel/ispfw.ko ]; then
	cp $BASE/boot/kernel/ispfw.ko $MNT/boot/kernel
    fi
    cp $BASE/boot/device.hints $MNT/boot
    cp $BASE/boot/loader.* $MNT/boot
    if [ -s $BASE/boot/mfsroot.gz ]; then
	cp $BASE/boot/mfsroot.gz $MNT/boot
    fi
    cp $BASE/boot/color.4th $MNT/boot
    cp $BASE/boot/support.4th $MNT/boot
    cp $BASE/boot/check-password.4th $MNT/boot
    cp $BASE/boot/screen.4th $MNT/boot
    mv $MNT/boot/loader.efi $MNT/efi/boot/bootia64.efi
    echo kern.cam.boot_delay=\"3000\" >> $MNT/boot/loader.conf
    echo vfs.root.mountfrom=\"cd9660:iso9660/$LABEL\" >> $MNT/boot/loader.conf
    umount $MNT
    mdconfig -d -u $md
    BOOTOPTS="-o bootimage=i386;$EFIPART -o no-emul-boot"
else
    BOOTOPTS=""
fi

publisher="The FreeBSD Project.  http://www.FreeBSD.org/"
echo "/dev/iso9660/$LABEL / cd9660 ro 0 0" > $BASE/etc/fstab
makefs -t cd9660 $BOOTOPTS -o rockridge -o label=$LABEL -o publisher="$publisher" $NAME $BASE $*
rm $BASE/etc/fstab
rm -f $EFIPART
exit 0
