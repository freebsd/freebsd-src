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
# Copyright 2014 Spectra Logic Corporation.
# Use is subject to license terms.
#

. $STF_SUITE/include/libtest.kshlib

verify_runnable "global"

function cleanup
{
	for wait_pid in $child_pids
	do
		$KILL $wait_pid
	done
}

function verify_assertion
{
	log_note "Busying the pool with: $DD if=/dev/urandom of=$TESTDIR/$TESTFILE bs=131072 count=8000000"
	$DD if=/dev/urandom of=$TESTDIR/$TESTFILE bs=131072 count=8000000 &
	typeset pid=$!

	if ! $PS -p $pid > /dev/null 2>&1; then
		log_fail "ERROR: $DD is no longer running"
	fi

	child_pids="$child_pids $pid"

	for disk in $DISKS; do
		log_must $ZPOOL offline $TESTPOOL $disk
		check_state $TESTPOOL $disk "offline"
		if [[ $? != 0 ]]; then
			log_fail "$disk of $TESTPOOL is not offline."
		fi

		log_must $ZPOOL online $TESTPOOL $disk
		check_state $TESTPOOL $disk "online"
		if [[ $? != 0 ]]; then
			log_fail "$disk of $TESTPOOL did not match online state"
		fi
	done

	for wait_pid in $child_pids
	do
		$KILL $wait_pid
	done

	typeset dir=$(get_device_dir $DISKS)
	verify_filesys "$TESTPOOL" "$TESTPOOL/$TESTFS" "$dir"
}

log_assert "Turning a disk offline and back online during I/O completes."
log_onexit cleanup

for keyword in "mirror" "raidz"; do
	typeset child_pid=""
	default_setup_noexit "$keyword $DISKS"
	verify_assertion
	destroy_pool $TESTPOOL
done

log_pass
