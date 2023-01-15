#!/bin/sh
# Copyright (c) 2020 Proofpoint, Inc. and its suppliers.
#	All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
# ----------------------------------------
# test ilenx() and xleni(), using t-ixlen
# ----------------------------------------

PRG=./t-ixlen
R=0
${PRG} <<EOF
1:1
3:123
1:ÿ1
1:ÿÿ
1:˜
1:ÿ˜
3:1ÿ˜2
4:ÿÿmqÿ˜
17:ÿÿmqÿ˜@sendmail.com
0:ÿ
1:ÿÿÿ
EOF
# note: the last two entries are not "valid" [i] strings,
# so the results could be considered bogus.
R=$?

${PRG} -x <<EOF
1:1
3:123
3:ÿ1
6:1ÿ˜2
EOF
R1=$?
[ $R -eq 0 ] && R=$R1

exit $R
