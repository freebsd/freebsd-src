#!/bin/sh
# Copyright (c) 2021 Proofpoint, Inc. and its suppliers.
#	All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
# ----------------------------------------
# test t-lockfile, analyze result
# ----------------------------------------

fail()
{
  echo "$0: $@"
  exit 1
}

PRG=./t-lockfile
O=l.log

analyze()
{
 # the "owner" unlock operation must be before
 # the "client" lock operation can succeed
 U=`grep -n 'owner=1, unlock.*done' $O | cut -d: -f1 | head -n1`
 [ x"$U" = "x" ] && U=`grep -n '_close' $O | cut -d: -f1 | head -n1`
 L=`grep -n 'owner=0, lock.* ok' $O | cut -d: -f1`
 [ x"$U" = "x" ] && return 1
 [ x"$L" = "x" ] && return 1
 [ $U -lt $L ]
}

all=true
while getopts 2a: FLAG
do
  case "${FLAG}" in
    2) all=false;;
    a) O=${OPTARG}
       analyze || fail "$opts: unlock1=$U, lock2=$L"
       exit;;
  esac
done
shift `expr ${OPTIND} - 1`

[ -x ${PRG} ] || fail "missing ${PRG}"

if $all
then
for opts in "" "-r" "-n" "-nr"
do
  ${PRG} $opts > $O 2>&1 || fail "$opts: $?"
  analyze || fail "$opts: unlock1=$U, lock2=$L"
done
fi

# try with two processes
for opts in "" "-r"
do
rm -f $O
${PRG} -W >> $O 2>&1 || fail "-W: $?"
wpid=$!
${PRG} -R $opts >> $O 2>&1 || fail "-R $opts: $?"
rpid=$!
analyze || fail "$opts: unlock1=$U, lock2=$L"
wait $wpid
wait $rpid
done

exit 0
