#!/bin/sh
# $FreeBSD$

D1="/usr/obj/nanobsd.rescue_i386"
D2="/usr/obj/nanobsd.rescue_amd64"

MD=`mdconfig -a -t vnode -f ${D1}/_.disk.full`

dd if=${D2}/_.disk.image of=/dev/${MD}s2 bs=128k

mdconfig -d -u ${MD}
