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
# ident	"@(#)hotspare_replace_007_pos.ksh	1.0	12/08/10 SL"
#
. $STF_SUITE/tests/hotspare/hotspare.kshlib
. $STF_SUITE/tests/zfsd/zfsd.kshlib
. $STF_SUITE/include/libsas.kshlib

################################################################################
#
# __stc_assertion_start
#
# ID: zfsd_autoreplace_002_pos
#
# DESCRIPTION: 
#	In a pool with the autoreplace property set, a vdev will be
#	replaced by physical path
#
# STRATEGY:
#	1. Create 1 storage pool without hot spares
#	2. Remove a vdev by disabling its SAS phy
#	3. Export the pool
#	4. Reenable the missing dev's SAS phy
#	5. Erase the missing dev's ZFS label
#	6. Disable the missing dev's SAS phy again
#	7. Import the pool
#	8. Reenable the missing dev's SAS phy
#	9. Verify that it does get added to the pool.
#
# TESTABILITY: explicit
#
# TEST_AUTOMATION_LEVEL: automated
#
# CODING STATUS: COMPLETED (2013-02-4)
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
	do_autoreplace
	# 9. Verify that it gets added to the pool
	for ((timeout=0; $timeout<10; timeout=$timeout+1)); do
		if check_state $TESTPOOL $REMOVAL_DISK "ONLINE"; then
			break
		fi
		$SLEEP 6
	done
	log_must check_state $TESTPOOL "$REMOVAL_DISK" "ONLINE"
}


typeset REMOVAL_DISK=$DISK0
typeset POOLDEVS="$DISK0 $DISK1 $DISK2 $DISK3"
set -A MY_KEYWORDS "mirror" "raidz1" "raidz2"
for keyword in "${MY_KEYWORDS[@]}" ; do
	log_must create_pool $TESTPOOL $keyword $POOLDEVS
	log_must poolexists "$TESTPOOL"
	log_must $ZPOOL set autoreplace=on $TESTPOOL
	verify_assertion
	destroy_pool "$TESTPOOL"
done
