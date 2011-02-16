#!/bin/sh
# $FreeBSD: src/tools/regression/zfs/zpool/add/doesnt_exist.t,v 1.1.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

dir=`dirname $0`
. ${dir}/../../misc.sh

echo "1..6"

disks_create 1
names_create 1

expect_fl ${ZPOOL} add ${name0} ${disk0}
expect_fl ${ZPOOL} status -x ${name0}
expect_fl ${ZPOOL} destroy ${name0}

expect_fl ${ZPOOL} add -f ${name0} ${disk0}
expect_fl ${ZPOOL} status -x ${name0}
expect_fl ${ZPOOL} destroy ${name0}

disks_destroy
