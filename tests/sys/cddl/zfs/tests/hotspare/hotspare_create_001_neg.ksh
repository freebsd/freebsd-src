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
# Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# ident	"@(#)hotspare_create_001_neg.ksh	1.5	09/06/22 SMI"
#
. $STF_SUITE/tests/hotspare/hotspare.kshlib

################################################################################
#
# __stc_assertion_start
#
# ID: hotspare_create_001_neg
#
# DESCRIPTION:
# 'zpool create [-f]' with hot spares will fail 
# while the hot spares belong to the following cases:
#	- existing pool
#	- nonexistent device,
#	- part of an active pool,
#	- currently mounted,
#	- a swap device,
#	- a dump device,
#	- identical with the basic vdev within the pool,
#
# STRATEGY:
# 1. Create case scenarios
# 2. For each scenario, try to create a new pool with hot spares 
# 	of the virtual devices
# 3. Verify the creation is failed.
#
# TESTABILITY: explicit
#
# TEST_AUTOMATION_LEVEL: automated
#
# CODING_STATUS: COMPLETED (2006-06-07)
#
# __stc_assertion_end
#
################################################################################

verify_runnable "global"

function cleanup
{
	for pool in $TESTPOOL $TESTPOOL1
	do
		destroy_pool $pool
	done

	log_onfail $UMOUNT $TMPDIR/mounted_dir
	log_onfail $SWAPOFF $swap_dev
	log_onfail $DUMPON -r $dump_dev

	partition_cleanup
}

log_assert "'zpool create [-f]' with hot spares should be failed " \
	"with inapplicable scenarios."
log_onexit cleanup

set_devs

mounted_dev=${DISK0}
swap_dev=${DISK1}
dump_dev=${DISK2}
nonexist_dev=${disk}sbad_slice_num

create_pool "$TESTPOOL" ${pooldevs[0]}

log_must $MKDIR $TMPDIR/mounted_dir
log_must $NEWFS $mounted_dev
log_must $MOUNT $mounted_dev $TMPDIR/mounted_dir

log_must $SWAPON $swap_dev

log_must $DUMPON $dump_dev

#
# Set up the testing scenarios parameters
#	- existing pool
#	- nonexistent device,
#	- part of an active pool,
#	- currently mounted,
#	- a swap device,
#	- identical with the basic vdev within the pool,

set -A arg "$TESTPOOL ${pooldevs[1]} spare ${pooldevs[2]}" \
	"$TESTPOOL1 ${pooldevs[1]} spare $nonexist_dev" \
	"$TESTPOOL1 ${pooldevs[1]} spare ${pooldevs[0]}" \
	"$TESTPOOL1 ${pooldevs[1]} spare $mounted_dev" \
	"$TESTPOOL1 ${pooldevs[1]} spare $swap_dev" \
	"$TESTPOOL1 ${pooldevs[1]} spare ${pooldevs[1]}"

typeset -i i=0
while (( i < ${#arg[*]} )); do
	log_mustnot $ZPOOL create ${arg[i]}
	log_mustnot $ZPOOL create -f ${arg[i]}
	(( i = i + 1 ))
done

#	- a dump device,
# https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=241070
# When that bug is fixed, add $dump_dev to $arg and remove this block.
log_must $ZPOOL create $TESTPOOL1 ${pooldevs[1]} spare $dump_dev
log_must $ZPOOL destroy -f $TESTPOOL1
log_must $ZPOOL create -f $TESTPOOL1 ${pooldevs[1]} spare $dump_dev
log_must $ZPOOL destroy -f $TESTPOOL1

# now destroy the pool to be polite
log_must $ZPOOL destroy -f $TESTPOOL

log_pass "'zpool create [-f]' with hot spare is failed as expected with inapplicable scenarios."
