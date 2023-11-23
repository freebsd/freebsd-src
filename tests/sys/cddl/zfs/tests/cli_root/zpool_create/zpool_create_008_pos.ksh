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
. $STF_SUITE/tests/cli_root/zpool_create/zpool_create.kshlib

################################################################################
#
# __stc_assertion_start
#
# ID: zpool_create_008_pos
#
# DESCRIPTION:
# 'zpool create' have to use '-f' scenarios
#
# STRATEGY:
# 1. Prepare the scenarios
# 2. Create pool without '-f' and verify it fails
# 3. Create pool with '-f' and verify it succeeds
#
# TESTABILITY: explicit
#
# TEST_AUTOMATION_LEVEL: automated
#
# CODING_STATUS: COMPLETED (2007-08-17)
#
# __stc_assertion_end
#
################################################################################

verify_runnable "global"

log_assert "'zpool create' have to use '-f' scenarios"

if [[ -n $DISK ]]; then
        disk=$DISK
else
        disk=$DISK0
fi

# Make the disk is EFI labeled first via pool creation
create_pool $TESTPOOL $disk
destroy_pool $TESTPOOL

# exported device to be as spare vdev need -f to create pool
log_must partition_disk $SIZE $disk 6
create_pool $TESTPOOL ${disk}p1 ${disk}p2
log_must $ZPOOL export $TESTPOOL
log_mustnot $ZPOOL create $TESTPOOL1 ${disk}p3 spare ${disk}p2 
create_pool $TESTPOOL1 ${disk}p3 spare ${disk}p2
destroy_pool $TESTPOOL1

log_pass "'zpool create' have to use '-f' scenarios"
