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
#
# ident	"@(#)zpool_status_001_neg.ksh	1.1	07/10/09 SMI"
#

. $STF_SUITE/include/libtest.kshlib

################################################################################
#
# __stc_assertion_start
#
# ID: zpool_status_001_neg
#
# DESCRIPTION:
#
# zpool status works when run as a user
#
# STRATEGY:
#
# 1. Run zpool status as a user
# 2. Verify we get output
#
# TESTABILITY: explicit
#
# TEST_AUTOMATION_LEVEL: automated
#
# CODING_STATUS: COMPLETED (2007-07-27)
#
# __stc_assertion_end
#
################################################################################

function check_pool_status
{
	RESULT=$($GREP "pool:" $TMPDIR/pool-status.${TESTCASE_ID})
	if [ -z "$RESULT" ]
	then
		log_fail "No pool: string found in zpool status output!"
	fi
	$RM $TMPDIR/pool-status.${TESTCASE_ID}
}

verify_runnable "global"

log_assert "zpool status works when run as a user"

log_must eval "$ZPOOL status > $TMPDIR/pool-status.${TESTCASE_ID}"
check_pool_status

log_must eval "$ZPOOL status -v > $TMPDIR/pool-status.${TESTCASE_ID}"
check_pool_status

log_must eval "$ZPOOL status $TESTPOOL> $TMPDIR/pool-status.${TESTCASE_ID}"
check_pool_status

log_must eval "$ZPOOL status -v $TESTPOOL > $TMPDIR/pool-status.${TESTCASE_ID}"
check_pool_status

# $TESTPOOL.virt has an offline device, so -x will show it
log_must eval "$ZPOOL status -x $TESTPOOL.virt > $TMPDIR/pool-status.${TESTCASE_ID}"
check_pool_status

log_pass "zpool status works when run as a user"

