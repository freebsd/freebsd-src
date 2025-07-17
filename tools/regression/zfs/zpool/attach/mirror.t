#!/bin/sh

dir=`dirname $0`
. ${dir}/../../misc.sh

echo "1..34"

disks_create 5
names_create 1

expect_ok ${ZPOOL} create ${name0} ${disk0}
expect_ok ${ZPOOL} attach ${name0} ${disk0} ${disk1}
wait_for_resilver ${name0}
exp=`(
  echo "  pool: ${name0}"
  echo " state: ONLINE"
  echo " scrub: resilver completed after [0-9]+h[0-9]+m with 0 errors on .*"
  echo "config:"
  echo "	NAME          STATE   READ WRITE CKSUM"
  echo "	${name0}      ONLINE     0     0     0"
  echo "	  mirror      ONLINE     0     0     0"
  echo "	    ${disk0}  ONLINE     0     0     0  [0-9.]+[A-Z] resilvered"
  echo "	    ${disk1}  ONLINE     0     0     0  [0-9.]+[A-Z] resilvered"
  echo "errors: No known data errors"
)`
expect "${exp}" ${ZPOOL} status ${name0}
expect_ok ${ZPOOL} attach ${name0} ${disk0} ${disk2}
wait_for_resilver ${name0}
exp=`(
  echo "  pool: ${name0}"
  echo " state: ONLINE"
  echo " scrub: resilver completed after [0-9]+h[0-9]+m with 0 errors on .*"
  echo "config:"
  echo "	NAME          STATE   READ WRITE CKSUM"
  echo "	${name0}      ONLINE     0     0     0"
  echo "	  mirror      ONLINE     0     0     0"
  echo "	    ${disk0}  ONLINE     0     0     0  [0-9.]+[A-Z] resilvered"
  echo "	    ${disk1}  ONLINE     0     0     0  [0-9.]+[A-Z] resilvered"
  echo "	    ${disk2}  ONLINE     0     0     0  [0-9.]+[A-Z] resilvered"
  echo "errors: No known data errors"
)`
expect "${exp}" ${ZPOOL} status ${name0}
expect_ok ${ZPOOL} attach ${name0} ${disk2} ${disk3}
wait_for_resilver ${name0}
exp=`(
  echo "  pool: ${name0}"
  echo " state: ONLINE"
  echo " scrub: resilver completed after [0-9]+h[0-9]+m with 0 errors on .*"
  echo "config:"
  echo "	NAME          STATE   READ WRITE CKSUM"
  echo "	${name0}      ONLINE     0     0     0"
  echo "	  mirror      ONLINE     0     0     0"
  echo "	    ${disk0}  ONLINE     0     0     0  [0-9.]+[A-Z] resilvered"
  echo "	    ${disk1}  ONLINE     0     0     0  [0-9.]+[A-Z] resilvered"
  echo "	    ${disk2}  ONLINE     0     0     0  [0-9.]+[A-Z] resilvered"
  echo "	    ${disk3}  ONLINE     0     0     0  [0-9.]+[A-Z] resilvered"
  echo "errors: No known data errors"
)`
expect "${exp}" ${ZPOOL} status ${name0}
expect_ok ${ZPOOL} detach ${name0} ${disk0}
exp=`(
  echo "  pool: ${name0}"
  echo " state: ONLINE"
  echo " scrub: resilver completed after [0-9]+h[0-9]+m with 0 errors on .*"
  echo "config:"
  echo "	NAME          STATE   READ WRITE CKSUM"
  echo "	${name0}      ONLINE     0     0     0"
  echo "	  mirror      ONLINE     0     0     0"
  echo "	    ${disk1}  ONLINE     0     0     0  [0-9.]+[A-Z] resilvered"
  echo "	    ${disk2}  ONLINE     0     0     0  [0-9.]+[A-Z] resilvered"
  echo "	    ${disk3}  ONLINE     0     0     0  [0-9.]+[A-Z] resilvered"
  echo "errors: No known data errors"
)`
expect "${exp}" ${ZPOOL} status ${name0}
expect_ok ${ZPOOL} detach ${name0} ${disk2}
exp=`(
  echo "  pool: ${name0}"
  echo " state: ONLINE"
  echo " scrub: resilver completed after [0-9]+h[0-9]+m with 0 errors on .*"
  echo "config:"
  echo "	NAME          STATE   READ WRITE CKSUM"
  echo "	${name0}      ONLINE     0     0     0"
  echo "	  mirror      ONLINE     0     0     0"
  echo "	    ${disk1}  ONLINE     0     0     0  [0-9.]+[A-Z] resilvered"
  echo "	    ${disk3}  ONLINE     0     0     0  [0-9.]+[A-Z] resilvered"
  echo "errors: No known data errors"
)`
expect "${exp}" ${ZPOOL} status ${name0}
expect_ok ${ZPOOL} detach ${name0} ${disk3}
exp=`(
  echo "  pool: ${name0}"
  echo " state: ONLINE"
  echo " scrub: resilver completed after [0-9]+h[0-9]+m with 0 errors on .*"
  echo "config:"
  echo "	NAME        STATE   READ WRITE CKSUM"
  echo "	${name0}    ONLINE     0     0     0"
  echo "	  ${disk1}  ONLINE     0     0     0  [0-9.]+[A-Z] resilvered"
  echo "errors: No known data errors"
)`
expect "${exp}" ${ZPOOL} status ${name0}
expect_fl ${ZPOOL} detach ${name0} ${disk1}
exp=`(
  echo "  pool: ${name0}"
  echo " state: ONLINE"
  echo " scrub: resilver completed after [0-9]+h[0-9]+m with 0 errors on .*"
  echo "config:"
  echo "	NAME        STATE   READ WRITE CKSUM"
  echo "	${name0}    ONLINE     0     0     0"
  echo "	  ${disk1}  ONLINE     0     0     0  [0-9.]+[A-Z] resilvered"
  echo "errors: No known data errors"
)`
expect "${exp}" ${ZPOOL} status ${name0}
expect_ok ${ZPOOL} destroy ${name0}
expect_fl ${ZPOOL} status -x ${name0}

expect_ok ${ZPOOL} create ${name0} ${disk0}
expect_ok ${ZPOOL} attach ${name0} ${disk0} ${disk1}
wait_for_resilver ${name0}
exp=`(
  echo "  pool: ${name0}"
  echo " state: ONLINE"
  echo " scrub: resilver completed after [0-9]+h[0-9]+m with 0 errors on .*"
  echo "config:"
  echo "	NAME          STATE   READ WRITE CKSUM"
  echo "	${name0}      ONLINE     0     0     0"
  echo "	  mirror      ONLINE     0     0     0"
  echo "	    ${disk0}  ONLINE     0     0     0  [0-9.]+[A-Z] resilvered"
  echo "	    ${disk1}  ONLINE     0     0     0  [0-9.]+[A-Z] resilvered"
  echo "errors: No known data errors"
)`
expect "${exp}" ${ZPOOL} status ${name0}
expect_ok ${ZPOOL} export ${name0}
expect_ok ${ZPOOL} import ${import_flags} ${name0}
exp=`(
  echo "  pool: ${name0}"
  echo " state: ONLINE"
  echo " scrub: none requested"
  echo "config:"
  echo "	NAME          STATE   READ WRITE CKSUM"
  echo "	${name0}      ONLINE     0     0     0"
  echo "	  mirror      ONLINE     0     0     0"
  echo "	    ${disk0}  ONLINE     0     0     0"
  echo "	    ${disk1}  ONLINE     0     0     0"
  echo "errors: No known data errors"
)`
expect "${exp}" ${ZPOOL} status ${name0}
expect_ok ${ZPOOL} destroy ${name0}
expect_fl ${ZPOOL} status -x ${name0}

expect_ok ${ZPOOL} create ${name0} ${disk0} ${disk1}
expect_ok ${ZPOOL} attach ${name0} ${disk0} ${disk2}
wait_for_resilver ${name0}
exp=`(
  echo "  pool: ${name0}"
  echo " state: ONLINE"
  echo " scrub: resilver completed after [0-9]+h[0-9]+m with 0 errors on .*"
  echo "config:"
  echo "	NAME          STATE   READ WRITE CKSUM"
  echo "	${name0}      ONLINE     0     0     0"
  echo "	  mirror      ONLINE     0     0     0"
  echo "	    ${disk0}  ONLINE     0     0     0  [0-9.]+[A-Z] resilvered"
  echo "	    ${disk2}  ONLINE     0     0     0  [0-9.]+[A-Z] resilvered"
  echo "	  ${disk1}    ONLINE     0     0     0"
  echo "errors: No known data errors"
)`
expect "${exp}" ${ZPOOL} status ${name0}
expect_ok ${ZPOOL} attach ${name0} ${disk1} ${disk3}
wait_for_resilver ${name0}
exp=`(
  echo "  pool: ${name0}"
  echo " state: ONLINE"
  echo " scrub: resilver completed after [0-9]+h[0-9]+m with 0 errors on .*"
  echo "config:"
  echo "	NAME          STATE   READ WRITE CKSUM"
  echo "	${name0}      ONLINE     0     0     0"
  echo "	  mirror      ONLINE     0     0     0"
  echo "	    ${disk0}  ONLINE     0     0     0"
  echo "	    ${disk2}  ONLINE     0     0     0"
  echo "	  mirror      ONLINE     0     0     0"
  echo "	    ${disk1}  ONLINE     0     0     0  [0-9.]+[A-Z] resilvered"
  echo "	    ${disk3}  ONLINE     0     0     0  [0-9.]+[A-Z] resilvered"
  echo "errors: No known data errors"
)`
expect "${exp}" ${ZPOOL} status ${name0}
expect_ok ${ZPOOL} attach ${name0} ${disk0} ${disk4}
wait_for_resilver ${name0}
exp=`(
  echo "  pool: ${name0}"
  echo " state: ONLINE"
  echo " scrub: resilver completed after [0-9]+h[0-9]+m with 0 errors on .*"
  echo "config:"
  echo "	NAME          STATE   READ WRITE CKSUM"
  echo "	${name0}      ONLINE     0     0     0"
  echo "	  mirror      ONLINE     0     0     0"
  echo "	    ${disk0}  ONLINE     0     0     0  [0-9.]+[A-Z] resilvered"
  echo "	    ${disk2}  ONLINE     0     0     0  [0-9.]+[A-Z] resilvered"
  echo "	    ${disk4}  ONLINE     0     0     0  [0-9.]+[A-Z] resilvered"
  echo "	  mirror      ONLINE     0     0     0"
  echo "	    ${disk1}  ONLINE     0     0     0"
  echo "	    ${disk3}  ONLINE     0     0     0"
  echo "errors: No known data errors"
)`
expect "${exp}" ${ZPOOL} status ${name0}
expect_ok ${ZPOOL} destroy ${name0}
expect_fl ${ZPOOL} status -x ${name0}

disks_destroy
