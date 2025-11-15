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
# Copyright 2025 ConnectWise.  All rights reserved.
# Use is subject to license terms.

. $STF_SUITE/tests/hotspare/hotspare.kshlib

verify_runnable "global"

function cleanup
{
	$ZPOOL status $TESTPOOL
	if poolexists $TESTPOOL ; then
		destroy_pool $TESTPOOL
	fi

	partition_cleanup
}

function verify_assertion
{
	log_must $ZPOOL offline $TESTPOOL $FAULT_DISK

	# Wait a few seconds before verifying the state
	$SLEEP 10
	log_must check_state $TESTPOOL "$FAULT_DISK" "OFFLINE"
	log_must check_state $TESTPOOL "$SPARE_DISK" "AVAIL"
}

log_onexit cleanup

log_assert "ZFSD will not automatically activate a spare when a disk has been administratively offlined"

ensure_zfsd_running

typeset FAULT_DISK=$DISK0
typeset SPARE_DISK=$DISK3
typeset POOLDEVS="$DISK0 $DISK1 $DISK2"
set -A MY_KEYWORDS mirror raidz1
for keyword in "${MY_KEYWORDS[@]}" ; do
	log_must create_pool $TESTPOOL $keyword $POOLDEVS spare $SPARE_DISK
	verify_assertion

	destroy_pool "$TESTPOOL"
done
