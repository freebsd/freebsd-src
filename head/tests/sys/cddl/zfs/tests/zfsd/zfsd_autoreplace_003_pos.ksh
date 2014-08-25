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
# Copyright 2014 Spectra Logic.  All rights reserved.
# Use is subject to license terms.
#
#
. $STF_SUITE/tests/hotspare/hotspare.kshlib
. $STF_SUITE/tests/zfsd/zfsd.kshlib
. $STF_SUITE/include/libsas.kshlib

################################################################################
#
# __stc_assertion_start
#
# ID: zfsd_autoreplace_003_pos
#
# DESCRIPTION: 
#	In a pool with the autoreplace property set, a vdev will be
#	replaced by physical path even if a spare is already active for that
#	vdev
#
# STRATEGY:
#	1. Create 1 storage pool with a hot spare
#	2. Remove a vdev by disabling its SAS phy
#	3. Wait for the hotspare to fully resilver
#	4. Export the pool
#	5. Reenable the missing dev's SAS phy
#	6. Erase the missing dev's ZFS label
#	7. Disable the missing dev's SAS phy again
#	8. Import the pool
#	9. Reenable the missing dev's SAS phy
#	10. Verify that it does get added to the pool.
#	11. Verify that the hotspare gets removed.
#
# TESTABILITY: explicit
#
# TEST_AUTOMATION_LEVEL: automated
#
# CODING STATUS: COMPLETED (2013-05-13)
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


log_assert "A pool with the autoreplace property will replace disks by physical path"

log_onexit cleanup

function verify_assertion
{
	do_autoreplace "$SPARE_DISK"
	# Verify that the original disk gets added to the pool
	for ((timeout=0; $timeout<10; timeout=$timeout+1)); do
		if check_state $TESTPOOL $REMOVAL_DISK "ONLINE"; then
			break
		fi
		$SLEEP 6
	done
	log_must check_state $TESTPOOL "$REMOVAL_DISK" "ONLINE"

	# Wait for resilvering to complete
	wait_until_resilvered

	# Check that the spare is deactivated
	log_must check_state $TESTPOOL "$SPARE_DISK" "AVAIL"
}


typeset SPARE_DISK=$DISK0
typeset REMOVAL_DISK=$DISK1
typeset POOLDEVS="$DISK1 $DISK2 $DISK3 $DISK4"
set -A MY_KEYWORDS "mirror" "raidz1" "raidz2"
for keyword in "${MY_KEYWORDS[@]}" ; do
	log_must create_pool $TESTPOOL $keyword $POOLDEVS spare $SPARE_DISK
	log_must poolexists "$TESTPOOL"
	log_must $ZPOOL set autoreplace=on $TESTPOOL
	verify_assertion
	destroy_pool "$TESTPOOL"
done
