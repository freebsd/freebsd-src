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
. $STF_SUITE/tests/cli_user/cli_user.kshlib

################################################################################
#
# __stc_assertion_start
#
# ID: zpool_remove_001_neg
#
# DESCRIPTION:
#
# zpool remove returns an error when run as a user
#
# STRATEGY:
# 1. Attempt to remove a device from a pool
# 2. Verify the command fails
#
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

verify_runnable "global"

log_assert "zpool remove returns an error when run as a user"

log_mustnot run_unprivileged "$ZPOOL remove $TESTPOOL.virt /$TESTDIR/disk-spare1.dat"

RESULT=$($ZPOOL status -v $TESTPOOL.virt | $GREP disk-spare1.dat)
if [ -z "$RESULT" ]
then
	log_fail "A disk was removed from the pool!"
fi


log_pass "zpool remove returns an error when run as a user"
