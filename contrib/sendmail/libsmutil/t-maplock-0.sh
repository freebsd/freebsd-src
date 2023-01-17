#!/bin/sh
# Copyright (c) 2021 Proofpoint, Inc. and its suppliers.
#	All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
# ----------------------------------------
# test map locking.
# Note: this is mostly for systems which use fcntl().
# just invoke it from the obj.*/libsmutil/ directory;
# otherwise use the -l and -m options to specify the paths.
# ----------------------------------------

fail()
{
  echo "$0: $@"
  exit 1
}

err()
{
  echo "$0: $@"
  rc=1
}

O=`basename $0`.0
V=vt
M=../makemap/makemap
CHKL=./t-lockfile

usage()
{
  cat <<EOF
$0: test basic makemap locking;
requires `basename ${CHKL}` and `basename ${M}`.
usage:
$0 [options]
options:
-l locktest   path to `basename ${CHKL}` [default: ${CHKL}]
-m makemap    path to `basename ${M}` [default: $M]
EOF
}

tries=0
rc=0
while getopts l:m:t: FLAG
do
  case "${FLAG}" in
    l) CHKL="${OPTARG}";;
    m) M="${OPTARG}";;
    t) tries="${OPTARG}";;
    *) usage
	exit 69
	;;
  esac
done
shift `expr ${OPTIND} - 1`

[ -x $M ] || fail "missing $M"
[ -x ${CHKL} ] || fail "missing ${CHKL}"

MAPTX=`$M -x | egrep 'hash|cdb'`

mm()
{
  (echo "l1 l2"; sleep 5; echo "e1 e2") |
  $M -v $MT $F >> $O 2>&1
}

chkl()
{
  ${CHKL} -Rrc -f $F >> $O 2>&1
}

for XT in ${MAPTX}
do

MT=`echo $XT | cut -d: -f1`
EXT=`echo $XT | cut -d: -f2`

F=$V.${EXT}

rm -f $O
mm &
wpid=$!
sleep 1
chkl&
rpid=$!

while [ $tries -gt 0 ]
do
  sleep 1; chkl
  tries=`expr $tries - 1 `
done

wait $wpid
wait $rpid

if grep "status=unknown" $O >/dev/null
then
  :
else
  # get the makemap pid, not the "mm" pid, for checks?
  grep "status=locked pid=" $O || err "$MT map not locked"
fi

done

exit $rc
