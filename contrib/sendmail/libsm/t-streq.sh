#!/bin/sh
# Copyright (c) 2020 Proofpoint, Inc. and its suppliers.
#	All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
# ----------------------------------------
# test SM_STRNCASEEQ
# ----------------------------------------

PRG=./t-streq
R=0
# format:
# two lines:
# len:string1
# result:string2
# result:
# 1: equal
# 0: not equal
${PRG} <<EOF
0:a
1:X
1:a
1:A
2:a
1:A
1:aB
1:AC
2:aB
0:AC
2:aB\n
1:AB
20:xabcez@uabcey.por.az\n
1:xabcez@uabcey.por.az
7:ünchen\n
1:ünchen
7:ünchen\n
0:üncheX
22:iseadmin@somest.sld.br>\n
1:iseadmin@somest.sld.br
22:iseadmin@somest.sld.br
1:iseadmin@somest.sld.br>\n
EOF
R=$?

exit $R
