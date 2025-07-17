#!/bin/sh

dir=`dirname $0`
. ${dir}/../../misc.sh

echo "1..19"

disks_create 5
names_create 1

expect_ok ${ZPOOL} create ${name0} ${disk0}
expect_fl ${ZPOOL} add ${name0} ${disk0}
expect_ok ${ZPOOL} destroy ${name0}

expect_ok ${ZPOOL} create ${name0} ${disk0} ${disk1}
expect_fl ${ZPOOL} add ${name0} ${disk0}
expect_fl ${ZPOOL} add ${name0} ${disk1}
expect_ok ${ZPOOL} destroy ${name0}

expect_ok ${ZPOOL} create ${name0} ${disk0}
expect_ok ${ZPOOL} add ${name0} ${disk1}
exp=`(
  echo "  pool: ${name0}"
  echo " state: ONLINE"
  echo " scrub: none requested"
  echo "config:"
  echo "	NAME        STATE   READ WRITE CKSUM"
  echo "	${name0}    ONLINE     0     0     0"
  echo "	  ${disk0}  ONLINE     0     0     0"
  echo "	  ${disk1}  ONLINE     0     0     0"
  echo "errors: No known data errors"
)`
expect "${exp}" ${ZPOOL} status ${name0}
expect_ok ${ZPOOL} destroy ${name0}
expect_fl ${ZPOOL} status -x ${name0}

expect_ok ${ZPOOL} create ${name0} ${disk0} ${disk1} ${disk2}
expect_ok ${ZPOOL} add ${name0} ${disk3} ${disk4}
expect_ok ${ZPOOL} status -x ${name0}
expect "pool '${name0}' is healthy" ${ZPOOL} status -x ${name0}
exp=`(
  echo "  pool: ${name0}"
  echo " state: ONLINE"
  echo " scrub: none requested"
  echo "config:"
  echo "	NAME        STATE   READ WRITE CKSUM"
  echo "	${name0}    ONLINE     0     0     0"
  echo "	  ${disk0}  ONLINE     0     0     0"
  echo "	  ${disk1}  ONLINE     0     0     0"
  echo "	  ${disk2}  ONLINE     0     0     0"
  echo "	  ${disk3}  ONLINE     0     0     0"
  echo "	  ${disk4}  ONLINE     0     0     0"
  echo "errors: No known data errors"
)`
expect "${exp}" ${ZPOOL} status ${name0}
expect_ok ${ZPOOL} destroy ${name0}
expect_fl ${ZPOOL} status -x ${name0}

disks_destroy
