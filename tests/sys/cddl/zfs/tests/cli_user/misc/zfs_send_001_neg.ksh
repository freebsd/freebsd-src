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
# ID: zfs_send_001_neg
#
# DESCRIPTION:
#
# zfs send returns an error when run as a user
#
# STRATEGY:
# 1. Attempt to send a dataset to a file
# 2. Verify the file created has zero-size
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

function cleanup
{
	if [ -e $TMPDIR/zfstest_datastream.${TESTCASE_ID} ]
	then
		log_must $RM $TMPDIR/zfstest_datastream.${TESTCASE_ID}
	fi
}

log_assert "zfs send returns an error when run as a user"
log_onexit cleanup

run_unprivileged "$ZFS send $TESTPOOL/$TESTFS@snap" > $TMPDIR/zfstest_datastream.${TESTCASE_ID} && log_fail "zfs send unexpectedly succeeded!"

# Now check that the above command actually did nothing

# We should have a non-zero-length file in $TMPDIR
if [ -s $TMPDIR/zfstest_datastream.${TESTCASE_ID} ]
then
	log_fail "A zfs send file was created in $TMPDIR/zfstest_datastream.${TESTCASE_ID} !"
fi

log_pass "zfs send returns an error when run as a user"

