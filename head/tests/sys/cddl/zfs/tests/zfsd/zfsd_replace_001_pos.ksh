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
# Use is subject to license terms.
#
# Copyright 2012,2013 Spectra Logic Corporation.  All rights reserved.
# Use is subject to license terms.
# 
# Portions taken from:
# ident	"@(#)replacement_001_pos.ksh	1.4	08/02/27 SMI"
#
# $Id: //SpectraBSD/stable/tests/sys/cddl/zfs/tests/zfsd/zfsd_replace_001_pos.ksh#2 $
# $FreeBSD$

. $STF_SUITE/tests/hotspare/hotspare.kshlib
. $STF_SUITE/tests/zfsd/zfsd.kshlib
. $STF_SUITE/include/libtest.kshlib
. $STF_SUITE/include/libsas.kshlib

verify_runnable "global"

log_assert "Failing a disk from a SAS expander is recognized by ZFS"

log_onexit autoreplace_cleanup

child_pids=""

set -A TMPDISKS $DISKS
typeset REMOVAL_DISK=${TMPDISKS[0]}
REMOVAL_DISK=${REMOVAL_DISK##*/}

for type in "raidz" "mirror"; do
	# Create a pool on the supplied disks
	create_pool $TESTPOOL $type $DISKS
	log_must $ZFS create $TESTPOOL/$TESTFS
	log_must $ZFS set mountpoint=$TESTDIR $TESTPOOL/$TESTFS

	# Find the first disk, get the expander and phy
	log_note "Looking for expander and phy information for $REMOVAL_DISK"
	find_verify_sas_disk $REMOVAL_DISK

	log_note "Disabling \"$REMOVAL_DISK\" on expander $EXPANDER phy $PHY"
	# Disable the first disk.  We have to do this first, because if
	# there is I/O active to the
	disable_sas_disk $EXPANDER $PHY

	# Check to make sure disk is gone.
	camcontrol inquiry $REMOVAL_DISK > /dev/null 2>&1
	if [ $? = 0 ]; then
		log_fail "Disk \"$REMOVAL_DISK\" was not removed"
	fi

	# Write out data to make sure we can do I/O after the disk failure
	# XXX KDM should check the status returned from the dd instances
	log_must dd if=/dev/zero of=$TESTDIR/$TESTFILE bs=1m count=512

	# We waited for the child processes to complete, so they're done.
	child_pids=""

	# Check to make sure ZFS sees the disk as removed
	$ZPOOL status $TESTPOOL |grep $REMOVAL_DISK |egrep -q 'REMOVED'
	if [ $? != 0 ]; then
		$ZPOOL status $TESTPOOL
		log_fail "disk $REMOVAL_DISK not listed as removed"
	fi

	# Make sure that the pool is degraded
	$ZPOOL status $TESTPOOL |grep "state:" |grep DEGRADED > /dev/null
	if [ $? != 0 ]; then
		$ZPOOL status $TESTPOOL
		log_fail "Pool $TESTPOOL not listed as DEGRADED"
	fi

	# Re-enable the disk, we don't want to leave it turned off
	log_note "Re-enabling phy $PHY on expander $EXPANDER"
	enable_sas_disk $EXPANDER $PHY

	log_note "Checking to see whether disk has reappeared"
	# Make sure the disk is back in the topology
	find_disk_by_phy $EXPANDER $PHY
	((retries=0))
	while [ -z "$FOUNDDISK" ] && [ "$retries" -lt 10 ]; do
		$SLEEP 5
		find_disk_by_phy $EXPANDER $PHY
	done

	if [ -z "$FOUNDDISK" ]; then
		camcontrol $EXPANDER
		log_fail "Disk $REMOVAL_DISK has not appeared at phy $PHY on expander $EXPANDER after 50 seconds"
	else
		log_note "Disk $REMOVAL_DISK is back as $FOUNDDISK"
	fi

	log_note "Raid type is $type"

	#Disk should have auto-joined the zpool. Verify it's status is online.
	$ZPOOL status |grep $REMOVAL_DISK |grep ONLINE > /dev/null
	if [ $? != 0 ]; then
		$ZPOOL status $TESTPOOL
		log_fail "disk $REMOVAL_DISK did not automatically join the $TESTPOOL"
	else 
		log_note "After reinsertion, disk is back in pool and online"
	fi

	#Make sure auto resilver has begun
	((retries=5))
        $ZPOOL status $TESTPOOL | egrep "scan:.*(resilver.in.progress|resilvered)" > /dev/null
	while [ $? != 0 ] && [ "$retries" -gt 0 ]; do
		log_note "Waiting for autoresilver to start: Retry $retries"
		$SLEEP 2
		((retries--))
                $ZPOOL status $TESTPOOL | egrep "scan:.*(resilver.in.progress|resilvered)" > /dev/null
	done

        $ZPOOL status $TESTPOOL | egrep "scan:.*(resilver.in.progress|resilvered)" > /dev/null
	if [ $? != 0 ]; then
		$ZPOOL status $TESTPOOL
		log_fail "Pool $TESTPOOL did not auto-resilver"
	else
		log_note "Auto-resilver of disk has started sucessfully."
	fi

	#Wait, then verify resilver done, and that pool optimal
	wait_until_resilvered

	$ZPOOL status $TESTPOOL
	destroy_pool $TESTPOOL
	log_must $RM -rf /$TESTPOOL
done

log_pass
