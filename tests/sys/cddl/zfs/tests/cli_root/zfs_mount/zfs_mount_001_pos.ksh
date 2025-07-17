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
. $STF_SUITE/tests/cli_root/zfs_mount/zfs_mount.kshlib

#################################################################################
#
# __stc_assertion_start
#
# ID: zfs_mount_001_pos
#
# DESCRIPTION:
# Invoke "zfs mount <filesystem>" with a regular name of filesystem,
# will mount that filesystem successfully.
#
# STRATEGY:
# 1. Make sure that the ZFS filesystem is unmounted.
# 2. Invoke 'zfs mount <filesystem>'.
# 3. Verify that the filesystem is mounted.
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

verify_runnable "both"

function cleanup
{
	log_must force_unmount $TESTPOOL/$TESTFS
	return 0
}

log_assert "Verify that '$ZFS $mountcmd <filesystem>' succeeds as root."

log_onexit cleanup

unmounted $TESTPOOL/$TESTFS || \
	log_must cleanup

log_must $ZFS $mountcmd $TESTPOOL/$TESTFS

log_note "Make sure the filesystem $TESTPOOL/$TESTFS is mounted"
mounted $TESTPOOL/$TESTFS || \
	log_fail Filesystem $TESTPOOL/$TESTFS is unmounted

log_pass "'$ZFS $mountcmd <filesystem>' succeeds as root."
