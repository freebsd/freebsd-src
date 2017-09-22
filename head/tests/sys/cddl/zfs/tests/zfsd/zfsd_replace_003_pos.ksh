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
# $FreeBSD$

. $STF_SUITE/include/libtest.kshlib
. $STF_SUITE/include/libsas.kshlib
. $STF_SUITE/tests/hotspare/hotspare.kshlib
. $STF_SUITE/tests/zfsd/zfsd.kshlib

verify_runnable "global"

function cleanup
{
	# See if the phy has been disabled, and try to re-enable it if possible.
	[ -n "$EXPANDER0" -a -n "$PHY0" ] && enable_sas_disk $EXPANDER0 $PHY0
	[ -n "$EXPANDER1" -a -n "$PHY1" ] && enable_sas_disk $EXPANDER1 $PHY1
	[ -n "$EXPANDER" -a -n "$PHY" ] && enable_sas_disk $EXPANDER $PHY

	destroy_pool $TESTPOOL
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

	# Check to make sure ZFS sees the disk as removed
	wait_for_pool_dev_state_change 20 $DISK "REMOVED|UNAVAIL"
}

# arg1: disk's old devname
# arg2: disk's expander's devname
# arg3: disk's phy number
# arg4: whether the devname must differ after reconnecting
function reconnect_disk
{
	typeset DISK=$1
	typeset EXPANDER=$2
	typeset PHY=$3

	# Re-enable the disk, we don't want to leave it turned off
	log_note "Re-enabling phy $PHY on expander $EXPANDER"
	enable_sas_disk $EXPANDER $PHY

	log_note "Checking to see whether disk has reappeared"

	prev_disk=$(find_disks $DISK)
	cur_disk=$(find_disks $FOUNDDISK)

	# If you get this, the test must be fixed to guarantee that
	# it will reappear with a different name.
	[ "${prev_disk}" = "${cur_disk}" ] && log_unsupported \
		"Disk $DISK reappeared with the same devname."

	#Disk should have auto-joined the zpool. Verify it's status is online.
	wait_for_pool_dev_state_change 20 $FOUNDDISK ONLINE
}

log_assert "ZFSD will correctly replace disks that disappear and reappear \
	   with different devnames"

# Outline
# Create a double-parity pool
# Remove two disks by disabling their SAS phys
# Reenable the phys in the opposite order
# Check that the disks's devnames have swapped
# Verify that the pool regains its health

log_onexit cleanup
ensure_zfsd_running

child_pids=""

set -A DISKS_ARRAY $DISKS
typeset DISK0=${DISKS_ARRAY[0]}
typeset DISK1=${DISKS_ARRAY[1]}
if [ ${DISK0##/dev/da} -gt ${DISK1##/dev/da} ]; then
	# Swap disks so we'll disable the lowest numbered first
	typeset TMP="$DISK1"
	DISK1="$DISK0"
	DISK0="$TMP"
fi

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
	wait_until_resilvered
	destroy_pool $TESTPOOL
	log_must $RM -rf /$TESTPOOL
done

log_pass
