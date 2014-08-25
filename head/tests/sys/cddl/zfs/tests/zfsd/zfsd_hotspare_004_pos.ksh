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

function cleanup
{
	poolexists $TESTPOOL && \
		destroy_pool $TESTPOOL

	# See if the phy has been disabled, and try to re-enable it if possible.
	if [ ! -z "$REMOVAL_DISK" ]; then
		camcontrol inquiry $REMOVAL_DISK > /dev/null
		if [ $? != 0 ]; then
			if [ ! -z "$EXPANDER" ] && [ ! -z "$PHY" ]; then
				enable_sas_disk $EXPANDER $PHY
			fi
		fi
	fi

	[[ -e $TESTDIR ]] && log_must $RM -rf $TESTDIR/*

	partition_cleanup
}


log_assert "Removing a disk from a pool results in the spare activating"

log_onexit cleanup


function verify_assertion # spare_dev
{
	typeset sdev=$1
	find_verify_sas_disk $REMOVAL_DISK
	log_note "Disabling \"$REMOVAL_DISK\" on expander $EXPANDER phy $PHY"
	disable_sas_disk $EXPANDER $PHY

	#Check to make sure the disk is gone
	camcontrol inquiry $REMOVAL_DISK > /dev/null 2>&1
	if [ $? = 0 ]; then
		log_fail "Disk \"$REMOVAL_DISK\" was not removed"
	fi

	# Check to make sure ZFS sees the disk as removed
	for ((timeout=0; $timeout<20; timeout=$timeout+1)); do
		check_state $TESTPOOL "$REMOVAL_DISK" "REMOVED"
		is_removed=$?
		if [[ $is_removed == 0 ]]; then
			break
		fi
		$SLEEP 3
	done
	log_must check_state $TESTPOOL "$REMOVAL_DISK" "REMOVED"

	# Check that the spare was activated
	for ((timeout=0; $timeout<20; timeout=$timeout+1)); do
		check_state $TESTPOOL "$sdev" "INUSE"
		spare_inuse=$?
		if [[ $spare_inuse == 0 ]]; then
			break
		fi
		$SLEEP 3
	done
	log_must $ZPOOL status $TESTPOOL
	log_must check_state $TESTPOOL "$sdev" "INUSE"

	# Reenable the  missing disk
	log_note "Reenabling phy on expander $EXPANDER phy $PHY"
	enable_sas_disk $EXPANDER $PHY

	# Check that the disk has returned
	for ((timeout=0; $timeout<20; timeout=$timeout+1)); do
		find_disk_by_phy $EXPANDER $PHY
		if [[ -n "$FOUNDDISK" ]]; then
			break
		fi
		$SLEEP 3
	done

	if [[ -z "$FOUNDDISK" ]]; then
		log_fail "Disk $REMOVAL_DISK never reappeared"
	fi

	#Check that the disk has rejoined the pool
	for ((timeout=0; $timeout<20; timeout=$timeout+1)); do
		check_state $TESTPOOL $REMOVAL_DISK ONLINE
		rejoined=$?
		if [[ $rejoined == 0 ]]; then
			break
		fi
		$SLEEP 3
	done
	log_must check_state $TESTPOOL $REMOVAL_DISK ONLINE

	#Check that the pool resilvered
	while ! is_pool_resilvered $TESTPOOL; do
		$SLEEP 2
	done
	log_must is_pool_resilvered $TESTPOOL
	log_must $ZPOOL status

	#Finally, check that the spare deactivated
	for ((timeout=0; $timeout<20; timeout=$timeout+1)); do
		check_state $TESTPOOL "$sdev" "AVAIL"
		deactivated=$?
		if [[ $deactivated == 0 ]]; then
			break
		fi
		$SLEEP 3
	done
	log_must check_state $TESTPOOL "$sdev" "AVAIL"
}


typeset REMOVAL_DISK=$DISK0
typeset SDEV=$DISK4
typeset POOLDEVS="$DISK0 $DISK1 $DISK2 $DISK3"
set -A MY_KEYWORDS "mirror" "raidz1" "raidz2"
for keyword in "${MY_KEYWORDS[@]}" ; do
	log_must create_pool $TESTPOOL $keyword $POOLDEVS spare $SDEV
	log_must poolexists "$TESTPOOL"
	log_must $ZPOOL set autoreplace=on "$TESTPOOL"
	iterate_over_hotspares verify_assertion $SDEV

	destroy_pool "$TESTPOOL"
done
