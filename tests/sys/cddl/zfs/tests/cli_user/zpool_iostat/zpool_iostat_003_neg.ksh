#!/usr/local/bin/ksh93 -p
#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#

#
# Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
. $STF_SUITE/include/libtest.kshlib

################################################################################
#
# __stc_assertion_start
#
# ID: zpool_iostat_003_neg
#
# DESCRIPTION:
# Executing 'zpool iostat' command with bad options fails.
#
# STRATEGY:
# 1. Create an array of badly formed 'zpool iostat' options.
# 2. Execute each element of the array.
# 3. Verify an error code is returned.
#
# TESTABILITY: explicit
#
# TEST_AUTOMATION_LEVEL: automated
#
# CODING_STATUS: COMPLETED (2007-06-18)
#
# __stc_assertion_end
#
################################################################################

verify_runnable "both"

typeset testpool
if is_global_zone ; then
        testpool=$TESTPOOL
else
        testpool=${TESTPOOL%%/*}
fi

set -A args "" "-?" "-f" "nonexistpool" "$TESTPOOL/$TESTFS" \
	"$testpool 1.23" "$testpool 0" "$testpool -1" "$testpool 1 0" \
	"$testpool 0 0"

log_assert "Executing 'zpool iostat' with bad options fails"

typeset -i i=1
while [[ $i -lt ${#args[*]} ]]; do
	log_mustnot $ZPOOL iostat ${args[i]}
	((i = i + 1))
done

log_pass "Executing 'zpool iostat' with bad options fails"
