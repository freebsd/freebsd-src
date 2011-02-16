#!/bin/sh
# $FreeBSD: src/tools/regression/zfs/zpool/create/option-R.t,v 1.1.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

dir=`dirname $0`
. ${dir}/../../misc.sh

echo "1..9"

disks_create 1
names_create 2

expect_fl is_mountpoint /${name0}
expect_fl is_mountpoint /${name1}
expect_ok ${ZPOOL} create -R /${name1} ${name0} ${disk0}
exp=`(
  echo "NAME      PROPERTY  VALUE      SOURCE"
  echo "${name0}  altroot   /${name1}  local"
)`
expect "${exp}" ${ZPOOL} get altroot ${name0}
expect_fl is_mountpoint /${name0}
if [ -z "${no_mountpoint}" ]; then
	expect_ok is_mountpoint /${name1}
else
	expect_fl is_mountpoint /${name1}
fi
expect_ok ${ZPOOL} destroy ${name0}
expect_fl is_mountpoint /${name0}
expect_fl is_mountpoint /${name1}

disks_destroy
