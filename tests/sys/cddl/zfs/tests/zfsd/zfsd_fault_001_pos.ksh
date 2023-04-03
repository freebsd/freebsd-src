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
# Copyright (c) 2012,2013 Spectra Logic Corporation.  All rights reserved.
# Use is subject to license terms.
# 

. $STF_SUITE/include/libtest.kshlib
. $STF_SUITE/include/libgnop.kshlib

################################################################################
#
# __stc_assertion_start
#
# ID: zfsd_fault_001_pos
#
# DESCRIPTION: 
#   If a vdev experiences IO errors, it will become faulted.
#       
#
# STRATEGY:
#   1. Create a storage pool.  Use gnop vdevs so we can inject I/O errors.
#   2. Inject IO errors while doing IO to the pool.
#   3. Verify that the vdev becomes FAULTED.
#   4. ONLINE it and verify that it resilvers and joins the pool.
#
# TESTABILITY: explicit
#
# TEST_AUTOMATION_LEVEL: automated
#
# CODING STATUS: COMPLETED (2012-08-09)
#
# __stc_assertion_end
#
###############################################################################

verify_runnable "global"

log_assert "ZFS will fault a vdev that produces IO errors"

ensure_zfsd_running

DISK0_NOP=${DISK0}.nop
DISK1_NOP=${DISK1}.nop

log_must create_gnops $DISK0 $DISK1

for type in "raidz" "mirror"; do
	log_note "Testing raid type $type"

	# Create a pool on the supplied disks
	create_pool $TESTPOOL $type "$DISK0_NOP" "$DISK1_NOP"
	log_must $ZFS create $TESTPOOL/$TESTFS

	# Cause some IO errors writing to the pool
	while true; do
		log_must gnop configure -e 5 -w 100 "$DISK1_NOP"
		$DD if=/dev/zero bs=128k count=1 >> \
			/$TESTPOOL/$TESTFS/$TESTFILE 2> /dev/null
		$FSYNC /$TESTPOOL/$TESTFS/$TESTFILE
		# Check to see if the pool is faulted yet
		$ZPOOL status $TESTPOOL | grep -q 'state: DEGRADED'
		if [ $? == 0 ]
		then
			log_note "$TESTPOOL got degraded"
			break
		fi
	done

	log_must check_state $TESTPOOL $TMPDISK "FAULTED"

	# Heal and reattach the failed disk
	log_must gnop configure -w 0 "$DISK1_NOP"
	log_must $ZPOOL online $TESTPOOL "$DISK1_NOP"

	# Verify that the pool resilvers and goes to the ONLINE state
	for (( retries=60; $retries>0; retries=$retries+1 ))
	do
		$ZPOOL status $TESTPOOL | egrep -q "scan:.*resilvered"
		RESILVERED=$?
		$ZPOOL status $TESTPOOL | egrep -q "state:.*ONLINE"
		ONLINE=$?
		if test $RESILVERED -a $ONLINE
		then
			break
		fi
		$SLEEP 2
	done

	if [ $retries == 0 ]
	then
		log_fail "$TESTPOOL never resilvered in the allowed time"
	fi

	destroy_pool $TESTPOOL
	log_must $RM -rf /$TESTPOOL
done

log_pass
