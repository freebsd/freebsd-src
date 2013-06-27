#!/bin/sh -x

tar xzpvf dist.tgz
rm -rf /boot/kernel
mv boot/kernel /boot/
mv usr/sbin/vpsctl /usr/sbin/vpsctl
mv sbin/mount_vpsfs /sbin/mount_vpsfs
mv usr/sbin/rsync_vps /usr/sbin/rsync_vps


