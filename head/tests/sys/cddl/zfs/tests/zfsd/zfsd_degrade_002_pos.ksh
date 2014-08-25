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
# Copyright (c) 2012-2014 Spectra Logic Corporation.  All rights reserved.
# Use is subject to license terms.
# 
# $Id: $
# $FreeBSD$

. $STF_SUITE/include/libtest.kshlib

################################################################################
#
# __stc_assertion_start
#
# ID: zfsd_degrade_001_pos
#
# DESCRIPTION: 
#   If an active hotspare experiences checksum errors, it will become degraded.
#       
#
# STRATEGY:
#   1. Create a storage pool with a hotspare.  Only use the file vdevs because
#      it is easy to generate checksum errors on them.
#   2. fault a vdev to active the hotspare
#   3. Mostly fill the pool with data.
#   4. Corrupt it by DDing to the hotspare's underlying file.
#   5. Verify that the hotspare becomes DEGRADED.
#   6. ONLINE it and verify that it resilvers and joins the pool.
#
# TESTABILITY: explicit
#
# TEST_AUTOMATION_LEVEL: automated
#
# CODING STATUS: COMPLETED (2014-05-13)
#
# __stc_assertion_end
#
###############################################################################

verify_runnable "global"

VDEV0=${TMPDIR}/file0.${TESTCASE_ID}
VDEV1=${TMPDIR}/file1.${TESTCASE_ID}
SPARE_VDEV=${TMPDIR}/file2.${TESTCASE_ID}
BASIC_VDEVS="${VDEV0} ${VDEV1}"
VDEVS="${BASIC_VDEVS} ${SPARE_VDEV}"
TESTFILE=/$TESTPOOL/testfile


function cleanup
{
	destroy_pool $TESTPOOL
	$RM -f $VDEVS
}

log_assert "ZFS will degrade a vdev that produces checksum errors"

log_onexit cleanup

log_must $MKFILE 100M ${VDEV0}
log_must $MKFILE 100M ${VDEV1}
log_must $MKFILE 100M ${SPARE_VDEV}


for type in "mirror" "raidz"; do
	log_note "Testing raid type $type"

	create_pool $TESTPOOL $type ${BASIC_VDEVS} spare ${SPARE_VDEV}

	# Activate the hotspare
	$ZINJECT -d ${VDEV0} -A fault $TESTPOOL

	# ZFSD can take up to 60 seconds to replace a failed device
	# (though it's usually faster).  
	for ((timeout=0; $timeout<10; timeout=$timeout+1)); do
		check_state $TESTPOOL "$SPARE_VDEV" "INUSE"
		spare_inuse=$?
		if [[ $spare_inuse == 0 ]]; then
			break
		fi
		$SLEEP 6
	done
	log_must $ZPOOL status $TESTPOOL
	log_must check_state $TESTPOOL "$SPARE_VDEV" "ONLINE"

	# do some IO on the pool
	log_must $DD if=/dev/random of=$TESTFILE bs=512 count=4096
	# Scribble on the underlying file to corrupt the vdev
	log_must $DD if=/dev/zero bs=1024k count=64 conv=notrunc of=$SPARE_VDEV
		
	# Scrub the pool to detect the corruption
	$SYNC
	log_must $ZPOOL scrub $TESTPOOL
	while is_pool_scrubbing $TESTPOOL ; do
		$SLEEP 2
	done

	# ZFSD can take up to 60 seconds to degrade an array in response to
	# errors (though it's usually faster).  
	for ((timeout=0; $timeout<10; timeout=$timeout+1)); do
		check_state $TESTPOOL "$SPARE_VDEV" "DEGRADED"
		degraded=$?
		if [[ $degraded == 0 ]]; then
			break
		fi
		$SLEEP 6
	done
	log_must $ZPOOL status $TESTPOOL
	log_must check_state $TESTPOOL "$SPARE_VDEV" "DEGRADED"

	destroy_pool $TESTPOOL
done

cleanup
log_pass

