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
. $STF_SUITE/tests/hotspare/hotspare.kshlib

################################################################################
#
# __stc_assertion_start
#
# ID: hotspare_snapshot_002_pos
#
# DESCRIPTION: 
#	If a storage pool has activated hot spares,
#	create snapshot and detach the basic vdev,
#	the hot spare should become the functional device, and
#	the data in snapshot should keep integrity.
#
# STRATEGY:
#	1. Create a storage pool with hot spares activated.
#	2. Create some files, create a snapshot upon filesystem
#	3. Activate a spare device to the pool
#	4. Create some files, create an new snapshot upon filesystem
#	5. Do 'zpool detach' with the original device
#	6. Verify the 2 snapshots are all kept, and verify the data integrity within them.
#
# TESTABILITY: explicit
#
# TEST_AUTOMATION_LEVEL: automated
#
# CODING STATUS: COMPLETED (2006-06-07)
#
# __stc_assertion_end
#
###############################################################################

verify_runnable "global"

function cleanup
{
	poolexists $TESTPOOL && \
		destroy_pool $TESTPOOL

	partition_cleanup
}

function verify_assertion # dev
{
	typeset dev=$1
	typeset odev=${pooldevs[0]}

	log_must $CP $MYTESTFILE $mtpt/$TESTFILE0
	log_must $ZFS snapshot $TESTPOOL@snap.0

	log_must $ZPOOL replace $TESTPOOL $odev $dev

	log_must $CP $MYTESTFILE $mtpt/$TESTFILE1
	log_must $ZFS snapshot $TESTPOOL@snap.1

	log_must $SYNC

	log_must $ZPOOL detach $TESTPOOL $odev

	for file in \
		"$snaproot/snap.0/$TESTFILE0" \
		"$snaproot/snap.1/$TESTFILE0" \
		"$snaproot/snap.1/$TESTFILE1" ; do
		[[ ! -e $file ]] && \
			log_fail "$file missing after detach hotspare."
		checksum2=$($SUM $file | $AWK '{print $1}')
		[[ "$checksum1" != "$checksum2" ]] && \
			log_fail "Checksums differ ($checksum1 != $checksum2)"
	done

	log_must $ZFS destroy $TESTPOOL@snap.1
	log_must $ZFS destroy $TESTPOOL@snap.0

	log_must $ZPOOL add -f "$TESTPOOL" spare $odev
	log_must $ZPOOL replace "$TESTPOOL" $dev $odev
	log_must $SYNC
	log_must $ZPOOL detach "$TESTPOOL" $dev
	log_must $ZPOOL add -f "$TESTPOOL" spare $dev
}

log_assert "'zpool detach <pool> <vdev> ...' against basic vdev do no harm to snapshot." 

log_onexit cleanup

typeset mtpt="" snaproot=""

set_devs

checksum1=$($SUM $MYTESTFILE | $AWK '{print $1}')

for keyword in "${keywords[@]}" ; do
	setup_hotspares "$keyword"

	mtpt=$(get_prop mountpoint $TESTPOOL)
	snaproot="$mtpt/$(get_snapdir_name)"

	iterate_over_hotspares verify_assertion

	destroy_pool "$TESTPOOL"
done

log_pass "'zpool detach <pool> <vdev> ...' against basic vdev should do no harm to snapshot." 
