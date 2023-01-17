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
EOF
R=$?

exit $R
