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
#ident	"@(#)zpool_create_012_neg.ksh	1.1	07/05/25 SMI"
#
. $STF_SUITE/include/libtest.kshlib

################################################################################
#
# __stc_assertion_start
#
# ID: zpool_create_012_neg
#
#
# DESCRIPTION:
# 'zpool create' will fail with disk in swap
#
#
# STRATEGY:
# 1. Add a disk to swap
# 2. Try to create a pool on that disk.  It should fail.
#
# TESTABILITY: explicit
#
# TEST_AUTOMATION_LEVEL: automated
#
# CODING_STATUS: COMPLETED (2007-04-17)
#
# __stc_assertion_end
#
################################################################################

verify_runnable "global"

function cleanup
{
	$SWAPOFF $DISK0

}

log_assert "'zpool create' should fail with disk in swap."
log_onexit cleanup

log_must $SWAPON $DISK0
log_mustnot $ZPOOL create $TESTPOOL $DISK0

log_pass "'zpool create' cannot use a swap disk"
