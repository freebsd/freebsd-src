#!/bin/ksh -p
# SPDX-License-Identifier: CDDL-1.0
#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or https://opensource.org/licenses/CDDL-1.0.
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

#
# Copyright (c) 2013, 2016 by Delphix. All rights reserved.
#

. $STF_SUITE/tests/functional/slog/slog.kshlib

#
# DESCRIPTION:
#	Pool can survive when one of mirror log device get corrupted
#
# STRATEGY:
#	1. Create pool with mirror slog devices
#	2. Make corrupted on one disk
#	3. Verify the pool is fine
#

verify_runnable "global"

log_assert "Pool can survive when one of mirror log device get corrupted."
log_onexit cleanup
log_must setup

for type in "" "mirror" "raidz" "raidz2"
do
	for spare in "" "spare"
	do
		log_must zpool create $TESTPOOL $type $VDEV $spare $SDEV \
			log mirror $LDEV

		mntpnt=$(get_prop mountpoint $TESTPOOL)
		#
		# Create file in pool to trigger writing in slog devices
		#
		log_must dd if=/dev/urandom of=$mntpnt/testfile.$$ count=100

		ldev=$(random_get $LDEV)
		log_must mkfile $MINVDEVSIZE $ldev
		log_must zpool scrub $TESTPOOL

		log_must display_status $TESTPOOL
		log_must verify_slog_device $TESTPOOL $ldev 'UNAVAIL' 'mirror'

		log_must zpool destroy -f $TESTPOOL
	done
done

log_pass "Pool can survive when one of mirror log device get corrupted."
