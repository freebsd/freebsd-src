#!/bin/sh
#
# This script generates a "memstick image" for UEFI-capable systems.
#
# Prerequisites:
#  - 'make release'
#  - KERNCONF *must* be VT (or vt_efifb(4) compiled into the kernel)
#
# Note:  This only works for amd64, because i386 lacks the EFI boot bits.
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

dirsize=$(du -shLm ${1} | awk '{print $1}')
dirsize=$(( $(( $(( ${dirsize} + 256 )) * 1024 * 1024 )) ))
truncate -s ${dirsize} ${2}

unit=$(mdconfig -a -t vnode -f ${2})
gpart create -s mbr /dev/${unit}
gpart add -t '!0xEF' -s 32M /dev/${unit}
gpart add -t freebsd /dev/${unit}
gpart set -a active -i 2 /dev/${unit}
gpart bootcode -b ${1}/boot/boot0 /dev/${unit}
gpart create -s bsd -n 20 /dev/${unit}s2
gpart add -t freebsd-ufs /dev/${unit}s2
gpart bootcode -b ${1}/boot/boot /dev/${unit}s2
newfs_msdos /dev/${unit}s1
newfs -L rootfs /dev/${unit}s2a
mkdir -p ${1}/mnt
mount -t msdosfs /dev/${unit}s1 ${1}/mnt
mkdir -p ${1}/mnt/efi/boot
cp -p ${1}/boot/boot1.efi ${1}/mnt/efi/boot/BOOTx64.efi

while ! umount ${1}/mnt; do
	sleep 1
done

mkdir -p mnt
mount /dev/${unit}s2a mnt
tar -cf - -C ${1} . | tar -xf - -C mnt
echo "/dev/ufs/rootfs / ufs ro,noatime 1 1" > mnt/etc/fstab

while ! umount mnt; do
	sleep 1
done

# Default boot selection to MBR so systems that do not support UEFI
# do not fail to boot without human interaction.
boot0cfg -s 2 /dev/${unit}

mdconfig -d -u ${unit}
rmdir mnt

