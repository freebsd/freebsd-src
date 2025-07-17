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
. $STF_SUITE/tests/utils_test/utils_test.kshlib

################################################################################
#
# __stc_assertion_start
#
# ID: utils_test_006_pos
#
# DESCRIPTION:
# Ensure that the fsirand(1M) utility fails on a ZFS file system.
#
# STRATEGY:
# 1. Populate a ZFS file system with some files.
# 2. Run fsirand(1M) against the device.
# 3. Ensure it fails.
#
# TESTABILITY: explicit
#
# TEST_AUTOMATION_LEVEL: automated
#
# CODING_STATUS: COMPLETED (2005-07-04)
#
# __stc_assertion_end
#
################################################################################

verify_runnable "global"

function cleanup
{
	ismounted $TESTPOOL/$TESTFS
	(( $? != 0 )) && \
		log_must $ZFS mount $TESTPOOL/$TESTFS
	
	$RM -rf $TESTDIR/*
}

log_onexit cleanup

log_assert "Ensure that the fsirand(1M) utility fails on a ZFS file system."

populate_dir $TESTDIR/$TESTFILE $NUM_FILES $WRITE_COUNT $BLOCKSZ $DATA

log_must $ZFS unmount $TESTDIR

log_mustnot $FSIRAND /dev/${DISK}s0

log_pass "fsirand(1M) returned an error as expected."
