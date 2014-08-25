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
# $Id: //SpectraBSD/stable/cddl/tools/regression/stc/src/suites/fs/zfs/tests/functional/zfsd/zfsd_replace_003_pos.ksh#1 $
# $FreeBSD$

. $STF_SUITE/include/libtest.kshlib
. $STF_SUITE/include/libsas.kshlib

verify_runnable "global"

function cleanup
{
	# See if the phy has been disabled, and try to re-enable it if possible.
	if [[ -n $EXPANDER0 && -n $PHY0 ]]; then
		enable_sas_disk $EXPANDER0 $PHY0
	fi
	if [[ -n $EXPANDER1 && -n $PHY1 ]]; then
		enable_sas_disk $EXPANDER1 $PHY1
	fi
	if [[ -n $EXPANDER && -n $PHY ]]; then
		enable_sas_disk $EXPANDER $PHY
	fi

	if poolexists $TESTPOOL; then
		destroy_pool $TESTPOOL
	fi

	[[ -e $TESTDIR ]] && log_must $RM -rf $TESTDIR/*
}

# arg1: disk devname
# Leaves EXPANDER and PHY set appropriately
function remove_disk
{
	typeset DISK=$1
	# Find the first disk, get the expander and phy
	log_note "Looking for expander and phy information for $DISK"
	find_verify_sas_disk $DISK

	log_note "Disabling \"$DISK\" on expander $EXPANDER phy $PHY"
	# Disable the first disk.
	disable_sas_disk $EXPANDER $PHY

	# Check to make sure disk is gone.
	camcontrol inquiry $DISK > /dev/null 2>&1
	if [ $? = 0 ]; then
		log_fail "Disk \"$DISK\" was not removed"
	fi

	# Check to make sure ZFS sees the disk as removed
	$ZPOOL status $TESTPOOL |grep $DISK |grep REMOVED > /dev/null
	if [ $? != 0 ]; then
		log_fail "disk $DISK not listed as removed"
	fi
}

# arg1: disk's old devname
# arg2: disk's expander's devname
# arg3: disk's phy number
function reconnect_disk
{
	typeset DISK=$1
	typeset EXPANDER=$2
	typeset PHY=$3
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
		log_fail "A disk has not appeared at phy $PHY on expander $EXPANDER after 50 seconds"
	else
		log_note "Disk $DISK is back as $FOUNDDISK"
	fi
	if [[ $(find_disks $FOUNDDISK) = $(find_disks $DISK) ]]; then
		log_unsupported "Disk $DISK reappeared with the same devname.  The test must be fixed to guarantee that it will reappear with a different name"
	fi

	#Disk should have auto-joined the zpool. Verify it's status is online.
	if [[ $(get_device_state $TESTPOOL $FOUNDDISK "") != ONLINE ]]; then
		log_fail "disk $FOUNDDISK did not automatically join $TESTPOOL"
	else 
		log_note "After reinsertion, disk $FOUNDDISK is back in pool and online"
	fi

}

log_assert "ZFSD will correctly replace disks that dissapear and reappear with different devnames"
# Outline
# Create a double-parity pool
# Remove two disks by disabling their SAS phys
# Reenable the phys in the opposite order
# Check that the disks's devnames have swapped
# Verify that the pool regains its health

log_onexit cleanup

child_pids=""

set -A DISKS_ARRAY $DISKS
typeset DISK0=${DISKS_ARRAY[0]}
typeset DISK1=${DISKS_ARRAY[1]}

for type in "raidz2" "mirror"; do
	# Create a pool on the supplied disks
	create_pool $TESTPOOL $type $DISKS

	remove_disk $DISK0
	typeset EXPANDER0=$EXPANDER
	typeset PHY0=$PHY
	remove_disk $DISK1
	typeset EXPANDER1=$EXPANDER
	typeset PHY1=$PHY

	# Make sure that the pool is degraded
	$ZPOOL status $TESTPOOL |grep "state:" |grep DEGRADED > /dev/null
	if [ $? != 0 ]; then
		log_fail "Pool $TESTPOOL not listed as DEGRADED"
	fi

	reconnect_disk $DISK1 $EXPANDER1 $PHY1
	reconnect_disk $DISK0 $EXPANDER0 $PHY0

	#Wait, then verify resilver done, and that pool is optimal
	((retries=24))
	$ZPOOL status $TESTPOOL |grep "scan:" |grep "resilvered" > /dev/null
	while [ $? != 0  ] && [ "$retries" -gt 0 ]; do
		log_note "Retry $retries"
		$SLEEP 5
		((retries--))
		$ZPOOL status $TESTPOOL |grep "scan:" |grep "resilvered" > /dev/null
	done

	$ZPOOL status $TESTPOOL |grep "scan:" |grep "resilvered" > /dev/null
	if [ $? != 0 ]; then
		log_fail "Pool $TESTPOOL did not finish resilvering in 120 seconds"
	else
		log_note "Auto-resilver completed sucessfully"
	fi

	destroy_pool $TESTPOOL
	log_must $RM -rf /$TESTPOOL
done

log_pass
