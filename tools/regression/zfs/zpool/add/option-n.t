#!/bin/sh

dir=`dirname $0`
. ${dir}/../../misc.sh

echo "1..5"

disks_create 2
names_create 1

expect_ok ${ZPOOL} create ${name0} ${disk0}
exp=`(
  echo "would update '${name0}' to the following configuration:"
  echo "	${name0}"
  echo "	  ${disk0}"
  echo "	  ${disk1}"
)`
expect "${exp}" ${ZPOOL} add -n ${name0} ${disk1}
exp=`(
  echo "  pool: ${name0}"
  echo " state: ONLINE"
  echo " scrub: none requested"
  echo "config:"
  echo "	NAME        STATE   READ WRITE CKSUM"
  echo "	${name0}    ONLINE     0     0     0"
  echo "	  ${disk0}  ONLINE     0     0     0"
  echo "errors: No known data errors"
)`
expect "${exp}" ${ZPOOL} status ${name0}
expect_ok ${ZPOOL} destroy ${name0}
expect_fl ${ZPOOL} status -x ${name0}

disks_destroy
