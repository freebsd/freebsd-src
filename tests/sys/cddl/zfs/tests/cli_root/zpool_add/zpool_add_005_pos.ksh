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
# Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
. $STF_SUITE/include/libtest.kshlib
. $STF_SUITE/tests/cli_root/zpool_add/zpool_add.kshlib

################################################################################
#
# __stc_assertion_start
#
# ID: zpool_add_005_pos
#
# DESCRIPTION: 
#       'zpool add' should return fail if 
#	1. vdev is part of an active pool
# 	2. vdev is currently mounted
# 	3. vdev is in /etc/vfstab
#	3. vdev is specified as the dedicated dump device
#
# STRATEGY:
#	1. Create case scenarios
#	2. For each scenario, try to add the device to the pool
#	3. Verify the add operation get failed
#
# TESTABILITY: explicit
#
# TEST_AUTOMATION_LEVEL: automated
#
# CODING_STATUS: COMPLETED (2005-09-29)
#
# __stc_assertion_end
#
###############################################################################

verify_runnable "global"

set_disks

function cleanup
{
	poolexists "$TESTPOOL" && \
		destroy_pool "$TESTPOOL"
	poolexists "$TESTPOOL1" && \
		destroy_pool "$TESTPOOL1"

	log_onfail $UMOUNT $TMPDIR/mounted_dir
	log_onfail $SWAPOFF $swap_dev
	log_onfail $DUMPON -r $dump_dev
}

log_assert "'zpool add' should fail with inapplicable scenarios."

log_onexit cleanup

create_pool "$TESTPOOL" "${DISK0}"
log_must poolexists "$TESTPOOL"

create_pool "$TESTPOOL1" "${DISK1}"
log_must poolexists "$TESTPOOL1"

mounted_dev=${DISK2}
log_must $MKDIR $TMPDIR/mounted_dir
log_must $NEWFS $mounted_dev
log_must $MOUNT $mounted_dev $TMPDIR/mounted_dir

swap_dev=${DISK3}
log_must $SWAPON $swap_dev

dump_dev=${DISK4}
log_must $DUMPON $dump_dev

log_mustnot $ZPOOL add -f "$TESTPOOL" ${DISK1}

log_mustnot $ZPOOL add -f "$TESTPOOL" $mounted_dev

log_mustnot $ZPOOL add -f "$TESTPOOL" $swap_dev

# https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=241070
# When that bug is fixed, change this to log_mustnot.
log_must $ZPOOL add -f "$TESTPOOL" $dump_dev

log_pass "'zpool add' should fail with inapplicable scenarios."
