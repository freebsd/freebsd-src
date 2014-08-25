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
# Copyright 2013 Spectra Logic.  All rights reserved.
# Use is subject to license terms.
#
# ident	"@(#)zfsd_zfsd_002_pos.ksh	1.0	12/08/10 SL"
#
. $STF_SUITE/tests/hotspare/hotspare.kshlib
. $STF_SUITE/include/libsas.kshlib

################################################################################
#
# __stc_assertion_start
#
# ID: zfsd_import_001_pos
#
# DESCRIPTION: 
#   If a removed drive gets reinserted while the pool is exported, it will
#   replace its spare when reinserted.
#
#   This also applies to drives that get reinserted while the machine is
#   powered off.
#       
#
# STRATEGY:
#	1. Create 1 storage pools with hot spares.  Use disks instead of files
#	   because they can be removed.
#	2. Remove one disk by turning off its SAS phy.
#	3. Verify that the spare is in use.
#	4. Reinsert the vdev by enabling its phy
#	5. Verify that the vdev gets resilvered and the spare gets removed
#	6. Use additional zpool history data to verify that the pool
#	   finished resilvering _before_ zfsd detached the spare.
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

function cleanup
{
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

	poolexists "$TESTPOOL" && \
		destroy_pool "$TESTPOOL"


	partition_cleanup
}

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

	# Wait for zfsd to activate the spare
	for ((timeout=0; $timeout<20; timeout=$timeout+1)); do
		check_state $TESTPOOL "$sdev" "INUSE"
		spare_inuse=$?
		if [[ $spare_inuse == 0 ]]; then
			break
		fi
		$SLEEP 3
	done
	log_must check_state $TESTPOOL "$sdev" "INUSE"
	
	# Export the pool
	log_must $ZPOOL export $TESTPOOL

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

	# Import the pool
	log_must $ZPOOL import $TESTPOOL

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

	# Verify that the spare was detached after the scrub was complete
	# Note that resilvers and scrubs are recorded identically in zpool
	# history
	$ZPOOL history -i $TESTPOOL | awk '
		BEGIN {
			scrub_txg=0;
			detach_txg=0
		}
		/scrub done/ {
			split($6, s, "[:\\]]");
			t=s[2];
			scrub_txg = scrub_txg > t ? scrub_txg : t
		}
		/vdev detach/ {
			split($6, s, "[:\\]]");
			t=s[2];
			done_txg = done_txg > t ? done_txg : t
		}
		END {
			print("Scrub completed at txg", scrub_txg);
			print("Spare detached at txg", detach_txg);
			exit(detach_txg > scrub_txg)
		}'
	if [[ $? -ne 0 ]]; then
		log_fail "The spare detached before the resilver completed"
	fi
}




if ! $(is_physical_device $DISKS) ; then
	log_unsupported "This directory cannot be run on raw files."
fi

log_assert "If a removed drive gets reinserted while the pool is exported, it will replace its spare when reinserted."

log_onexit cleanup

set_devs

typeset REMOVAL_DISK=$DISK0
typeset SDEV=$DISK4
typeset POOLDEVS="$DISK0 $DISK1 $DISK2 $DISK3"
set -A MY_KEYWORDS "mirror" "raidz1" "raidz2"
for keyword in "${MY_KEYWORDS[@]}" ; do
	log_must create_pool $TESTPOOL $keyword $POOLDEVS spare $SDEV
	log_must poolexists "$TESTPOOL"
	iterate_over_hotspares verify_assertion $SDEV

	destroy_pool "$TESTPOOL"
done
