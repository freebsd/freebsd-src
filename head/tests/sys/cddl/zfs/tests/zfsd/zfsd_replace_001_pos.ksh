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

. $STF_SUITE/tests/hotspare/hotspare.kshlib
. $STF_SUITE/tests/zfsd/zfsd.kshlib
. $STF_SUITE/include/libtest.kshlib
. $STF_SUITE/include/libsas.kshlib

verify_runnable "global"

log_assert "Failing a disk from a SAS expander is recognized by ZFS"

log_onexit autoreplace_cleanup
ensure_zfsd_running

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

	# Write out data to make sure we can do I/O after the disk failure
	log_must $DD if=/dev/zero of=$TESTDIR/$TESTFILE bs=1m count=512

	# Check to make sure ZFS sees the disk as removed
	wait_for_pool_removal 20

	# Re-enable the disk, we don't want to leave it turned off
	log_note "Re-enabling phy $PHY on expander $EXPANDER"
	enable_sas_disk $EXPANDER $PHY

	# Disk should auto-join the zpool & be resilvered.
	wait_for_pool_dev_state_change 20 $REMOVAL_DISK ONLINE
	wait_until_resilvered

	$ZPOOL status $TESTPOOL
	destroy_pool $TESTPOOL
	log_must $RM -rf /$TESTPOOL
done

log_pass
