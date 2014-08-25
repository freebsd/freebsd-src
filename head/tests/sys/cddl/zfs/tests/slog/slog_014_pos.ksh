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
#
# ident	"@(#)slog_014_pos.ksh	1.1	09/06/22 SMI"
#

. $STF_SUITE/tests/slog/slog.kshlib

#################################################################################
#
# __stc_assertion_start
#
# ID: slog_014_pos
#
# DESCRIPTION:
#	log device can survive when one of pool device get corrupted
#
# STRATEGY:
#	1. Create pool with slog devices
#	2. remove one disk
#	3. Verify the log is fine
#
# TESTABILITY: explicit
#
# TEST_AUTOMATION_LEVEL: automated
#
# CODING_STATUS: COMPLETED (2009-05-26)
#
# __stc_assertion_end
#
################################################################################

verify_runnable "global"

function cleanup
{
	if [[ ! -f $VDIR/a ]]; then
		$MKFILE $SIZE $VDIR/a
	fi
}

log_assert "log device can survive when one of the pool device get corrupted."
log_onexit cleanup

for type in "mirror" "raidz" "raidz2"
do
	for spare in "" "spare"
	do
		log_must $ZPOOL create $TESTPOOL $type $VDEV $spare $SDEV \
			log $LDEV 

		# remove one of the pool device to make the pool DEGRADED
		log_must $RM -f $VDIR/a
		# Export and import the pool to force a close() and open() of
		# the missing vdev file.
		log_must $ZPOOL export $TESTPOOL
		log_must $ZPOOL import -d $VDIR $TESTPOOL

		# Check and verify pool status
		log_must display_status $TESTPOOL
		log_must $ZPOOL status $TESTPOOL 2>&1 >/dev/null

		# Check that there is some status: field informing us of a
		# problem.  The exact error string is unspecified.
		$ZPOOL status -v $TESTPOOL | \
			$GREP "status:" 2>&1 >/dev/null
		if (( $? != 0 )); then
			log_fail "pool $TESTPOOL status should indicate a missing device"
		fi

		for l in $LDEV; do
			log_must check_state $TESTPOOL $l "ONLINE"
		done
		
		log_must $ZPOOL destroy -f $TESTPOOL
		log_must $MKFILE $SIZE $VDIR/a
	done
done

log_pass "log device can survive when one of the pool device get corrupted."
