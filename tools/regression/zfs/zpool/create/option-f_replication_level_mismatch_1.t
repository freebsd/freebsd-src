#!/bin/sh

dir=`dirname $0`
. ${dir}/../../misc.sh

echo "1..180"

disks_create 9
names_create 1

expect_fl ${ZPOOL} create ${name0} mirror ${disk0} ${disk1} mirror ${disk2} ${disk3} ${disk4}
expect_fl ${ZPOOL} status -x ${name0}
expect_fl ${ZPOOL} destroy ${name0}

expect_ok ${ZPOOL} create ${zpool_f_flag} ${name0} mirror ${disk0} ${disk1} mirror ${disk2} ${disk3} ${disk4}
expect_ok ${ZPOOL} status -x ${name0}
expect "pool '${name0}' is healthy" ${ZPOOL} status -x ${name0}
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
  echo "	  mirror      ONLINE     0     0     0"
  echo "	    ${disk2}  ONLINE     0     0     0"
  echo "	    ${disk3}  ONLINE     0     0     0"
  echo "	    ${disk4}  ONLINE     0     0     0"
  echo "errors: No known data errors"
)`
expect "${exp}" ${ZPOOL} status ${name0}
expect_ok ${ZPOOL} destroy ${name0}
expect_fl ${ZPOOL} status -x ${name0}
expect_fl ${ZPOOL} destroy ${name0}

expect_fl ${ZPOOL} create ${name0} mirror ${disk0} ${disk1} ${disk2} mirror ${disk3} ${disk4}
expect_fl ${ZPOOL} status -x ${name0}
expect_fl ${ZPOOL} destroy ${name0}

expect_ok ${ZPOOL} create ${zpool_f_flag} ${name0} mirror ${disk0} ${disk1} ${disk2} mirror ${disk3} ${disk4}
expect_ok ${ZPOOL} status -x ${name0}
expect "pool '${name0}' is healthy" ${ZPOOL} status -x ${name0}
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
  echo "	    ${disk2}  ONLINE     0     0     0"
  echo "	  mirror      ONLINE     0     0     0"
  echo "	    ${disk3}  ONLINE     0     0     0"
  echo "	    ${disk4}  ONLINE     0     0     0"
  echo "errors: No known data errors"
)`
expect "${exp}" ${ZPOOL} status ${name0}
expect_ok ${ZPOOL} destroy ${name0}
expect_fl ${ZPOOL} status -x ${name0}
expect_fl ${ZPOOL} destroy ${name0}

expect_fl ${ZPOOL} create ${name0} raidz ${disk0} ${disk1} ${disk2} raidz ${disk3} ${disk4} ${disk5} ${disk6}
expect_fl ${ZPOOL} status -x ${name0}
expect_fl ${ZPOOL} destroy ${name0}

expect_ok ${ZPOOL} create ${zpool_f_flag} ${name0} raidz ${disk0} ${disk1} ${disk2} raidz ${disk3} ${disk4} ${disk5} ${disk6}
expect_ok ${ZPOOL} status -x ${name0}
expect "pool '${name0}' is healthy" ${ZPOOL} status -x ${name0}
exp=`(
  echo "  pool: ${name0}"
  echo " state: ONLINE"
  echo " scrub: none requested"
  echo "config:"
  echo "	NAME          STATE   READ WRITE CKSUM"
  echo "	${name0}      ONLINE     0     0     0"
  echo "	  raidz1      ONLINE     0     0     0"
  echo "	    ${disk0}  ONLINE     0     0     0"
  echo "	    ${disk1}  ONLINE     0     0     0"
  echo "	    ${disk2}  ONLINE     0     0     0"
  echo "	  raidz1      ONLINE     0     0     0"
  echo "	    ${disk3}  ONLINE     0     0     0"
  echo "	    ${disk4}  ONLINE     0     0     0"
  echo "	    ${disk5}  ONLINE     0     0     0"
  echo "	    ${disk6}  ONLINE     0     0     0"
  echo "errors: No known data errors"
)`
expect "${exp}" ${ZPOOL} status ${name0}
expect_ok ${ZPOOL} destroy ${name0}
expect_fl ${ZPOOL} status -x ${name0}
expect_fl ${ZPOOL} destroy ${name0}

expect_fl ${ZPOOL} create ${name0} raidz ${disk0} ${disk1} ${disk2} ${disk3} raidz ${disk4} ${disk5} ${disk6}
expect_fl ${ZPOOL} status -x ${name0}
expect_fl ${ZPOOL} destroy ${name0}

expect_ok ${ZPOOL} create ${zpool_f_flag} ${name0} raidz ${disk0} ${disk1} ${disk2} ${disk3} raidz ${disk4} ${disk5} ${disk6}
expect_ok ${ZPOOL} status -x ${name0}
expect "pool '${name0}' is healthy" ${ZPOOL} status -x ${name0}
exp=`(
  echo "  pool: ${name0}"
  echo " state: ONLINE"
  echo " scrub: none requested"
  echo "config:"
  echo "	NAME          STATE   READ WRITE CKSUM"
  echo "	${name0}      ONLINE     0     0     0"
  echo "	  raidz1      ONLINE     0     0     0"
  echo "	    ${disk0}  ONLINE     0     0     0"
  echo "	    ${disk1}  ONLINE     0     0     0"
  echo "	    ${disk2}  ONLINE     0     0     0"
  echo "	    ${disk3}  ONLINE     0     0     0"
  echo "	  raidz1      ONLINE     0     0     0"
  echo "	    ${disk4}  ONLINE     0     0     0"
  echo "	    ${disk5}  ONLINE     0     0     0"
  echo "	    ${disk6}  ONLINE     0     0     0"
  echo "errors: No known data errors"
)`
expect "${exp}" ${ZPOOL} status ${name0}
expect_ok ${ZPOOL} destroy ${name0}
expect_fl ${ZPOOL} status -x ${name0}
expect_fl ${ZPOOL} destroy ${name0}

expect_fl ${ZPOOL} create ${name0} raidz2 ${disk0} ${disk1} ${disk2} ${disk3} raidz2 ${disk4} ${disk5} ${disk6} ${disk7} ${disk8}
expect_fl ${ZPOOL} status -x ${name0}
expect_fl ${ZPOOL} destroy ${name0}

expect_ok ${ZPOOL} create ${zpool_f_flag} ${name0} raidz2 ${disk0} ${disk1} ${disk2} ${disk3} raidz2 ${disk4} ${disk5} ${disk6} ${disk7} ${disk8}
expect_ok ${ZPOOL} status -x ${name0}
expect "pool '${name0}' is healthy" ${ZPOOL} status -x ${name0}
exp=`(
  echo "  pool: ${name0}"
  echo " state: ONLINE"
  echo " scrub: none requested"
  echo "config:"
  echo "	NAME          STATE   READ WRITE CKSUM"
  echo "	${name0}      ONLINE     0     0     0"
  echo "	  raidz2      ONLINE     0     0     0"
  echo "	    ${disk0}  ONLINE     0     0     0"
  echo "	    ${disk1}  ONLINE     0     0     0"
  echo "	    ${disk2}  ONLINE     0     0     0"
  echo "	    ${disk3}  ONLINE     0     0     0"
  echo "	  raidz2      ONLINE     0     0     0"
  echo "	    ${disk4}  ONLINE     0     0     0"
  echo "	    ${disk5}  ONLINE     0     0     0"
  echo "	    ${disk6}  ONLINE     0     0     0"
  echo "	    ${disk7}  ONLINE     0     0     0"
  echo "	    ${disk8}  ONLINE     0     0     0"
  echo "errors: No known data errors"
)`
expect "${exp}" ${ZPOOL} status ${name0}
expect_ok ${ZPOOL} destroy ${name0}
expect_fl ${ZPOOL} status -x ${name0}
expect_fl ${ZPOOL} destroy ${name0}

expect_fl ${ZPOOL} create ${name0} raidz2 ${disk0} ${disk1} ${disk2} ${disk3} ${disk4} raidz2 ${disk5} ${disk6} ${disk7} ${disk8}
expect_fl ${ZPOOL} status -x ${name0}
expect_fl ${ZPOOL} destroy ${name0}

expect_ok ${ZPOOL} create ${zpool_f_flag} ${name0} raidz2 ${disk0} ${disk1} ${disk2} ${disk3} ${disk4} raidz2 ${disk5} ${disk6} ${disk7} ${disk8}
expect_ok ${ZPOOL} status -x ${name0}
expect "pool '${name0}' is healthy" ${ZPOOL} status -x ${name0}
exp=`(
  echo "  pool: ${name0}"
  echo " state: ONLINE"
  echo " scrub: none requested"
  echo "config:"
  echo "	NAME          STATE   READ WRITE CKSUM"
  echo "	${name0}      ONLINE     0     0     0"
  echo "	  raidz2      ONLINE     0     0     0"
  echo "	    ${disk0}  ONLINE     0     0     0"
  echo "	    ${disk1}  ONLINE     0     0     0"
  echo "	    ${disk2}  ONLINE     0     0     0"
  echo "	    ${disk3}  ONLINE     0     0     0"
  echo "	    ${disk4}  ONLINE     0     0     0"
  echo "	  raidz2      ONLINE     0     0     0"
  echo "	    ${disk5}  ONLINE     0     0     0"
  echo "	    ${disk6}  ONLINE     0     0     0"
  echo "	    ${disk7}  ONLINE     0     0     0"
  echo "	    ${disk8}  ONLINE     0     0     0"
  echo "errors: No known data errors"
)`
expect "${exp}" ${ZPOOL} status ${name0}
expect_ok ${ZPOOL} destroy ${name0}
expect_fl ${ZPOOL} status -x ${name0}
expect_fl ${ZPOOL} destroy ${name0}

expect_fl ${ZPOOL} create ${name0} mirror ${disk0} ${disk1} raidz ${disk2} ${disk3}
expect_fl ${ZPOOL} status -x ${name0}
expect_fl ${ZPOOL} destroy ${name0}

expect_ok ${ZPOOL} create ${zpool_f_flag} ${name0} mirror ${disk0} ${disk1} raidz ${disk2} ${disk3}
expect_ok ${ZPOOL} status -x ${name0}
expect "pool '${name0}' is healthy" ${ZPOOL} status -x ${name0}
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
  echo "	  raidz1      ONLINE     0     0     0"
  echo "	    ${disk2}  ONLINE     0     0     0"
  echo "	    ${disk3}  ONLINE     0     0     0"
  echo "errors: No known data errors"
)`
expect "${exp}" ${ZPOOL} status ${name0}
expect_ok ${ZPOOL} destroy ${name0}
expect_fl ${ZPOOL} status -x ${name0}
expect_fl ${ZPOOL} destroy ${name0}

expect_fl ${ZPOOL} create ${name0} raidz ${disk0} ${disk1} mirror ${disk2} ${disk3}
expect_fl ${ZPOOL} status -x ${name0}
expect_fl ${ZPOOL} destroy ${name0}

expect_ok ${ZPOOL} create ${zpool_f_flag} ${name0} raidz ${disk0} ${disk1} mirror ${disk2} ${disk3}
expect_ok ${ZPOOL} status -x ${name0}
expect "pool '${name0}' is healthy" ${ZPOOL} status -x ${name0}
exp=`(
  echo "  pool: ${name0}"
  echo " state: ONLINE"
  echo " scrub: none requested"
  echo "config:"
  echo "	NAME          STATE   READ WRITE CKSUM"
  echo "	${name0}      ONLINE     0     0     0"
  echo "	  raidz1      ONLINE     0     0     0"
  echo "	    ${disk0}  ONLINE     0     0     0"
  echo "	    ${disk1}  ONLINE     0     0     0"
  echo "	  mirror      ONLINE     0     0     0"
  echo "	    ${disk2}  ONLINE     0     0     0"
  echo "	    ${disk3}  ONLINE     0     0     0"
  echo "errors: No known data errors"
)`
expect "${exp}" ${ZPOOL} status ${name0}
expect_ok ${ZPOOL} destroy ${name0}
expect_fl ${ZPOOL} status -x ${name0}
expect_fl ${ZPOOL} destroy ${name0}

expect_fl ${ZPOOL} create ${name0} mirror ${disk0} ${disk1} ${disk2} raidz ${disk3} ${disk4}
expect_fl ${ZPOOL} status -x ${name0}
expect_fl ${ZPOOL} destroy ${name0}

expect_ok ${ZPOOL} create ${zpool_f_flag} ${name0} mirror ${disk0} ${disk1} ${disk2} raidz ${disk3} ${disk4}
expect_ok ${ZPOOL} status -x ${name0}
expect "pool '${name0}' is healthy" ${ZPOOL} status -x ${name0}
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
  echo "	    ${disk2}  ONLINE     0     0     0"
  echo "	  raidz1      ONLINE     0     0     0"
  echo "	    ${disk3}  ONLINE     0     0     0"
  echo "	    ${disk4}  ONLINE     0     0     0"
  echo "errors: No known data errors"
)`
expect "${exp}" ${ZPOOL} status ${name0}
expect_ok ${ZPOOL} destroy ${name0}
expect_fl ${ZPOOL} status -x ${name0}
expect_fl ${ZPOOL} destroy ${name0}

expect_fl ${ZPOOL} create ${name0} mirror ${disk0} ${disk1} raidz ${disk2} ${disk3} ${disk4}
expect_fl ${ZPOOL} status -x ${name0}
expect_fl ${ZPOOL} destroy ${name0}

expect_ok ${ZPOOL} create ${zpool_f_flag} ${name0} mirror ${disk0} ${disk1} raidz ${disk2} ${disk3} ${disk4}
expect_ok ${ZPOOL} status -x ${name0}
expect "pool '${name0}' is healthy" ${ZPOOL} status -x ${name0}
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
  echo "	  raidz1      ONLINE     0     0     0"
  echo "	    ${disk2}  ONLINE     0     0     0"
  echo "	    ${disk3}  ONLINE     0     0     0"
  echo "	    ${disk4}  ONLINE     0     0     0"
  echo "errors: No known data errors"
)`
expect "${exp}" ${ZPOOL} status ${name0}
expect_ok ${ZPOOL} destroy ${name0}
expect_fl ${ZPOOL} status -x ${name0}
expect_fl ${ZPOOL} destroy ${name0}

expect_fl ${ZPOOL} create ${name0} mirror ${disk0} ${disk1} raidz2 ${disk2} ${disk3} ${disk4}
expect_fl ${ZPOOL} status -x ${name0}
expect_fl ${ZPOOL} destroy ${name0}

expect_ok ${ZPOOL} create ${zpool_f_flag} ${name0} mirror ${disk0} ${disk1} raidz2 ${disk2} ${disk3} ${disk4}
expect_ok ${ZPOOL} status -x ${name0}
expect "pool '${name0}' is healthy" ${ZPOOL} status -x ${name0}
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
  echo "	  raidz2      ONLINE     0     0     0"
  echo "	    ${disk2}  ONLINE     0     0     0"
  echo "	    ${disk3}  ONLINE     0     0     0"
  echo "	    ${disk4}  ONLINE     0     0     0"
  echo "errors: No known data errors"
)`
expect "${exp}" ${ZPOOL} status ${name0}
expect_ok ${ZPOOL} destroy ${name0}
expect_fl ${ZPOOL} status -x ${name0}
expect_fl ${ZPOOL} destroy ${name0}

expect_fl ${ZPOOL} create ${name0} mirror ${disk0} ${disk1} ${disk2} raidz2 ${disk3} ${disk4} ${disk5}
expect_fl ${ZPOOL} status -x ${name0}
expect_fl ${ZPOOL} destroy ${name0}

expect_ok ${ZPOOL} create ${zpool_f_flag} ${name0} mirror ${disk0} ${disk1} ${disk2} raidz2 ${disk3} ${disk4} ${disk5}
expect_ok ${ZPOOL} status -x ${name0}
expect "pool '${name0}' is healthy" ${ZPOOL} status -x ${name0}
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
  echo "	    ${disk2}  ONLINE     0     0     0"
  echo "	  raidz2      ONLINE     0     0     0"
  echo "	    ${disk3}  ONLINE     0     0     0"
  echo "	    ${disk4}  ONLINE     0     0     0"
  echo "	    ${disk5}  ONLINE     0     0     0"
  echo "errors: No known data errors"
)`
expect "${exp}" ${ZPOOL} status ${name0}
expect_ok ${ZPOOL} destroy ${name0}
expect_fl ${ZPOOL} status -x ${name0}
expect_fl ${ZPOOL} destroy ${name0}

expect_fl ${ZPOOL} create ${name0} mirror ${disk0} ${disk1} ${disk2} ${disk3} raidz2 ${disk4} ${disk5} ${disk6}
expect_fl ${ZPOOL} status -x ${name0}
expect_fl ${ZPOOL} destroy ${name0}

expect_ok ${ZPOOL} create ${zpool_f_flag} ${name0} mirror ${disk0} ${disk1} ${disk2} ${disk3} raidz2 ${disk4} ${disk5} ${disk6}
expect_ok ${ZPOOL} status -x ${name0}
expect "pool '${name0}' is healthy" ${ZPOOL} status -x ${name0}
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
  echo "	    ${disk2}  ONLINE     0     0     0"
  echo "	    ${disk3}  ONLINE     0     0     0"
  echo "	  raidz2      ONLINE     0     0     0"
  echo "	    ${disk4}  ONLINE     0     0     0"
  echo "	    ${disk5}  ONLINE     0     0     0"
  echo "	    ${disk6}  ONLINE     0     0     0"
  echo "errors: No known data errors"
)`
expect "${exp}" ${ZPOOL} status ${name0}
expect_ok ${ZPOOL} destroy ${name0}
expect_fl ${ZPOOL} status -x ${name0}
expect_fl ${ZPOOL} destroy ${name0}

expect_fl ${ZPOOL} create ${name0} raidz1 ${disk0} ${disk1} raidz2 ${disk2} ${disk3} ${disk4}
expect_fl ${ZPOOL} status -x ${name0}
expect_fl ${ZPOOL} destroy ${name0}

expect_ok ${ZPOOL} create ${zpool_f_flag} ${name0} raidz1 ${disk0} ${disk1} raidz2 ${disk2} ${disk3} ${disk4}
expect_ok ${ZPOOL} status -x ${name0}
expect "pool '${name0}' is healthy" ${ZPOOL} status -x ${name0}
exp=`(
  echo "  pool: ${name0}"
  echo " state: ONLINE"
  echo " scrub: none requested"
  echo "config:"
  echo "	NAME          STATE   READ WRITE CKSUM"
  echo "	${name0}      ONLINE     0     0     0"
  echo "	  raidz1      ONLINE     0     0     0"
  echo "	    ${disk0}  ONLINE     0     0     0"
  echo "	    ${disk1}  ONLINE     0     0     0"
  echo "	  raidz2      ONLINE     0     0     0"
  echo "	    ${disk2}  ONLINE     0     0     0"
  echo "	    ${disk3}  ONLINE     0     0     0"
  echo "	    ${disk4}  ONLINE     0     0     0"
  echo "errors: No known data errors"
)`
expect "${exp}" ${ZPOOL} status ${name0}
expect_ok ${ZPOOL} destroy ${name0}
expect_fl ${ZPOOL} status -x ${name0}
expect_fl ${ZPOOL} destroy ${name0}

expect_fl ${ZPOOL} create ${name0} raidz1 ${disk0} ${disk1} ${dik2} raidz2 ${disk3} ${disk4} ${disk5}
expect_fl ${ZPOOL} status -x ${name0}
expect_fl ${ZPOOL} destroy ${name0}

expect_ok ${ZPOOL} create ${zpool_f_flag} ${name0} raidz1 ${disk0} ${disk1} ${disk2} raidz2 ${disk3} ${disk4} ${disk5}
expect_ok ${ZPOOL} status -x ${name0}
expect "pool '${name0}' is healthy" ${ZPOOL} status -x ${name0}
exp=`(
  echo "  pool: ${name0}"
  echo " state: ONLINE"
  echo " scrub: none requested"
  echo "config:"
  echo "	NAME          STATE   READ WRITE CKSUM"
  echo "	${name0}      ONLINE     0     0     0"
  echo "	  raidz1      ONLINE     0     0     0"
  echo "	    ${disk0}  ONLINE     0     0     0"
  echo "	    ${disk1}  ONLINE     0     0     0"
  echo "	    ${disk2}  ONLINE     0     0     0"
  echo "	  raidz2      ONLINE     0     0     0"
  echo "	    ${disk3}  ONLINE     0     0     0"
  echo "	    ${disk4}  ONLINE     0     0     0"
  echo "	    ${disk5}  ONLINE     0     0     0"
  echo "errors: No known data errors"
)`
expect "${exp}" ${ZPOOL} status ${name0}
expect_ok ${ZPOOL} destroy ${name0}
expect_fl ${ZPOOL} status -x ${name0}
expect_fl ${ZPOOL} destroy ${name0}

expect_fl ${ZPOOL} create ${name0} raidz1 ${disk0} ${disk1} ${dik2} ${disk3} raidz2 ${disk4} ${disk5} ${disk6}
expect_fl ${ZPOOL} status -x ${name0}
expect_fl ${ZPOOL} destroy ${name0}

expect_ok ${ZPOOL} create ${zpool_f_flag} ${name0} raidz1 ${disk0} ${disk1} ${disk2} ${disk3} raidz2 ${disk4} ${disk5} ${disk6}
expect_ok ${ZPOOL} status -x ${name0}
expect "pool '${name0}' is healthy" ${ZPOOL} status -x ${name0}
exp=`(
  echo "  pool: ${name0}"
  echo " state: ONLINE"
  echo " scrub: none requested"
  echo "config:"
  echo "	NAME          STATE   READ WRITE CKSUM"
  echo "	${name0}      ONLINE     0     0     0"
  echo "	  raidz1      ONLINE     0     0     0"
  echo "	    ${disk0}  ONLINE     0     0     0"
  echo "	    ${disk1}  ONLINE     0     0     0"
  echo "	    ${disk2}  ONLINE     0     0     0"
  echo "	    ${disk3}  ONLINE     0     0     0"
  echo "	  raidz2      ONLINE     0     0     0"
  echo "	    ${disk4}  ONLINE     0     0     0"
  echo "	    ${disk5}  ONLINE     0     0     0"
  echo "	    ${disk6}  ONLINE     0     0     0"
  echo "errors: No known data errors"
)`
expect "${exp}" ${ZPOOL} status ${name0}
expect_ok ${ZPOOL} destroy ${name0}
expect_fl ${ZPOOL} status -x ${name0}
expect_fl ${ZPOOL} destroy ${name0}

add_msg="# TODO Sun CR 6726091, Lustre bug 16873"
expect_fl ${ZPOOL} create ${name0} ${disk0} log mirror ${disk1} ${disk2} mirror ${disk3} ${disk4} ${disk5}
expect_fl ${ZPOOL} status -x ${name0}
expect_fl ${ZPOOL} destroy ${name0}
add_msg=""

expect_ok ${ZPOOL} create ${zpool_f_flag} ${name0} ${disk0} log mirror ${disk1} ${disk2} mirror ${disk3} ${disk4} ${disk5}
expect_ok ${ZPOOL} status -x ${name0}
expect "pool '${name0}' is healthy" ${ZPOOL} status -x ${name0}
exp=`(
  echo "  pool: ${name0}"
  echo " state: ONLINE"
  echo " scrub: none requested"
  echo "config:"
  echo "	NAME          STATE   READ WRITE CKSUM"
  echo "	${name0}      ONLINE     0     0     0"
  echo "	  ${disk0}    ONLINE     0     0     0"
  echo "	logs          ONLINE     0     0     0"
  echo "	  mirror      ONLINE     0     0     0"
  echo "	    ${disk1}  ONLINE     0     0     0"
  echo "	    ${disk2}  ONLINE     0     0     0"
  echo "	  mirror      ONLINE     0     0     0"
  echo "	    ${disk3}  ONLINE     0     0     0"
  echo "	    ${disk4}  ONLINE     0     0     0"
  echo "	    ${disk5}  ONLINE     0     0     0"
  echo "errors: No known data errors"
)`
expect "${exp}" ${ZPOOL} status ${name0}
expect_ok ${ZPOOL} destroy ${name0}
expect_fl ${ZPOOL} status -x ${name0}
expect_fl ${ZPOOL} destroy ${name0}

add_msg="# TODO Sun CR 6726091, Lustre bug 16873"
expect_fl ${ZPOOL} create ${name0} ${disk0} log mirror ${disk1} ${disk2} ${disk3} mirror ${disk4} ${disk5}
expect_fl ${ZPOOL} status -x ${name0}
expect_fl ${ZPOOL} destroy ${name0}
add_msg=""

expect_ok ${ZPOOL} create ${zpool_f_flag} ${name0} ${disk0} log mirror ${disk1} ${disk2} ${disk3} mirror ${disk4} ${disk5}
expect_ok ${ZPOOL} status -x ${name0}
expect "pool '${name0}' is healthy" ${ZPOOL} status -x ${name0}
exp=`(
  echo "  pool: ${name0}"
  echo " state: ONLINE"
  echo " scrub: none requested"
  echo "config:"
  echo "	NAME          STATE   READ WRITE CKSUM"
  echo "	${name0}      ONLINE     0     0     0"
  echo "	  ${disk0}    ONLINE     0     0     0"
  echo "	logs          ONLINE     0     0     0"
  echo "	  mirror      ONLINE     0     0     0"
  echo "	    ${disk1}  ONLINE     0     0     0"
  echo "	    ${disk2}  ONLINE     0     0     0"
  echo "	    ${disk3}  ONLINE     0     0     0"
  echo "	  mirror      ONLINE     0     0     0"
  echo "	    ${disk4}  ONLINE     0     0     0"
  echo "	    ${disk5}  ONLINE     0     0     0"
  echo "errors: No known data errors"
)`
expect "${exp}" ${ZPOOL} status ${name0}
expect_ok ${ZPOOL} destroy ${name0}
expect_fl ${ZPOOL} status -x ${name0}
expect_fl ${ZPOOL} destroy ${name0}

disks_destroy
