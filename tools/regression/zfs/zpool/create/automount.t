#!/bin/sh
# $FreeBSD: src/tools/regression/zfs/zpool/create/automount.t,v 1.1.2.1.8.1 2012/03/03 06:15:13 kensmith Exp $

dir=`dirname $0`
. ${dir}/../../misc.sh

echo "1..5"

disks_create 1
names_create 1

expect_fl is_mountpoint /${name0}
expect_ok ${ZPOOL} create ${name0} ${disk0}
if [ -z "${no_mountpoint}" ]; then
	expect_ok is_mountpoint /${name0}
else
	expect_fl is_mountpoint /${name0}
fi
expect_ok ${ZPOOL} destroy ${name0}
expect_fl is_mountpoint /${name0}

disks_destroy
