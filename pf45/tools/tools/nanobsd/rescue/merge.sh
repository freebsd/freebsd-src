#!/bin/sh
# $FreeBSD$

D1="/usr/obj/nanobsd.rescue_i386"
D2="/usr/obj/nanobsd.rescue_amd64"

MD=`mdconfig -a -t vnode -f ${D1}/_.disk.full`

dd if=${D2}/_.disk.image of=/dev/${MD}s2 bs=128k
tunefs -L rescues2a /dev/${MD}s2a
mount /dev/${MD}s2a ${D1}/_.mnt

sed -i "" -e 's/rescues1/rescues2/' ${D1}/_.mnt/conf/base/etc/fstab
sed -i "" -e 's/rescues1/rescues2/' ${D1}/_.mnt/etc/fstab

umount ${D1}/_.mnt

mdconfig -d -u ${MD}
