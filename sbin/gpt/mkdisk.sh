#!/bin/sh
# $FreeBSD$

dd if=/dev/zero of=disk count=125307
sudo mdconfig -a -t vnode -f disk -u 4
sudo fdisk -f - md4 <<EOF
g c567 h13 s17
p 1 165 221 124865
EOF
sudo disklabel -w -r md4 pan60
sudo mdconfig -d -u 4
