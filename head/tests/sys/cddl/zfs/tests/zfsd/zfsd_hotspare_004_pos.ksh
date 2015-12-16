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
# Copyright 2012 Spectra Logic.  All rights reserved.
# Use is subject to license terms.
#
# ident	"@(#)hotspare_replace_006_pos.ksh	1.0	12/08/10 SL"
#
. $STF_SUITE/tests/hotspare/hotspare.kshlib
. $STF_SUITE/tests/zfsd/zfsd.kshlib
. $STF_SUITE/include/libsas.kshlib

################################################################################
#
# __stc_assertion_start
#
# ID: zfs_hotspare_004_pos
#
# DESCRIPTION: 
#	If a vdev gets removed from a pool with a spare, the spare will be
#	activated.
#       
#
# STRATEGY:
#	1. Create 1 storage pools with hot spares.  Use disks instead of files
#	   because they can be removed.
#	2. Remove one vdev by turning off its SAS phy.
#	3. Verify that the spare is in use.
#	4. Reinsert the vdev by enabling its phy
#	5. Verify that the vdev gets resilvered and the spare gets removed
#
# TESTABILITY: explicit
#
# TEST_AUTOMATION_LEVEL: automated
#
# CODING STATUS: COMPLETED (2012-08-10)
#
# __stc_assertion_end
#
###############################################################################

verify_runnable "global"
verify_disk_count "$DISKS" 5


log_assert "Removing a disk from a pool results in the spare activating"

log_onexit autoreplace_cleanup


function verify_assertion # spare_dev
{
	typeset spare_dev=$1
	find_verify_sas_disk $REMOVAL_DISK
	log_note "Disabling \"$REMOVAL_DISK\" on expander $EXPANDER phy $PHY"
	disable_sas_disk $EXPANDER $PHY

	# Check to make sure the disk is gone
	find_disk_by_phy $EXPANDER $PHY
	[ -n "$FOUNDDISK" ] && log_fail "Disk \"$REMOVAL_DISK\" was not removed"

	# Check to make sure ZFS sees the disk as removed
	wait_for_pool_removal 20

	# Check that the spare was activated
	wait_for_pool_dev_state_change 20 $spare_dev INUSE
	log_must $ZPOOL status $TESTPOOL

	# Reenable the  missing disk
	log_note "Reenabling phy on expander $EXPANDER phy $PHY"
	enable_sas_disk $EXPANDER $PHY
	wait_for_disk_to_reappear 20 $EXPANDER $PHY

	# Check that the disk has rejoined the pool & resilvered
	wait_for_pool_dev_state_change 20 $REMOVAL_DISK ONLINE
	wait_until_resilvered

	# Finally, check that the spare deactivated
	wait_for_pool_dev_state_change 20 $spare_dev AVAIL
}


typeset REMOVAL_DISK=$DISK0
typeset SDEV=$DISK4
typeset POOLDEVS="$DISK0 $DISK1 $DISK2 $DISK3"
set -A MY_KEYWORDS "mirror" "raidz1" "raidz2"
ensure_zfsd_running
for keyword in "${MY_KEYWORDS[@]}" ; do
	log_must create_pool $TESTPOOL $keyword $POOLDEVS spare $SDEV
	log_must poolexists "$TESTPOOL"
	log_must $ZPOOL set autoreplace=on "$TESTPOOL"
	iterate_over_hotspares verify_assertion $SDEV

	destroy_pool "$TESTPOOL"
done
