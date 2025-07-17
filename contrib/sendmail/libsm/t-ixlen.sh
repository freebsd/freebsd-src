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
1:�1
1:��
1:�
1:��
3:1��2
4:��mq��
17:��mq��@sendmail.com
0:�
1:���
EOF
# note: the last two entries are not "valid" [i] strings,
# so the results could be considered bogus.
R=$?

${PRG} -x <<EOF
1:1
3:123
3:�1
6:1��2
EOF
R1=$?
[ $R -eq 0 ] && R=$R1

exit $R
