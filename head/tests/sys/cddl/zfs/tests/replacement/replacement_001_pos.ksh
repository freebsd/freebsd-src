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
# Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
# Copyright 2014 Spectra Logic Corporation.
# Use is subject to license terms.
#

. $STF_SUITE/include/libtest.kshlib

verify_runnable "global"

function cleanup
{
	if [[ -n "$child_pids" ]]; then
		for wait_pid in $child_pids
		do
		        $KILL $wait_pid
		done
	fi
}

log_assert "Replacing a disk during I/O completes."

log_onexit cleanup

child_pids=""

function replace_test
{
	typeset opt=$1
	typeset disk1=$2
	typeset disk2=$3

	log_note "Busying the pool with: $DD if=/dev/urandom of=$TESTDIR/testfile bs=131072 count=8000000"
	$DD if=/dev/urandom of=$TESTDIR/testfile bs=131072 count=8000000 &
	typeset pid=$!
	if ! $PS -p $pid > /dev/null 2>&1; then
		log_fail "ERROR: $DD is no longer running"
	fi
	child_pids="$child_pids $pid"

	log_must $ZPOOL replace $opt $TESTPOOL $disk1 $disk2

	for wait_pid in $child_pids
	do
		$KILL $wait_pid
	done
	child_pids=""

        log_must $ZPOOL export $TESTPOOL
        log_must $ZPOOL import $TESTPOOL
        log_must $ZFS umount $TESTPOOL/$TESTFS
        log_must $ZDB -cdui $TESTPOOL/$TESTFS
        log_must $ZFS mount $TESTPOOL/$TESTFS

}

typeset -a DISKS_A=($DISKS)
typeset replacement_disk=${DISKS_A[2]}
typeset short_replacement_disk=${replacement_disk##/dev/}
for type in "" "raidz" "raidz1" "mirror"; do
	for opt in "" "-f"; do
		create_pool $TESTPOOL $type ${DISKS_A[@]:0:2}
		log_must $ZFS create $TESTPOOL/$TESTFS
		log_must $ZFS set mountpoint=$TESTDIR $TESTPOOL/$TESTFS

		replace_test "$opt" ${DISKS_A[0]} $replacement_disk

		$ZPOOL iostat -v $TESTPOOL | grep -q $short_replacement_disk
		if [[ $? -ne 0 ]]; then
			log_fail "$replacement_disk is not present."
		fi

		destroy_pool $TESTPOOL
		log_must $RM -rf /$TESTPOOL
	done
done

log_pass
