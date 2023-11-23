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
. $STF_SUITE/tests/hotspare/hotspare.kshlib

################################################################################
#
# __stc_assertion_start
#
# ID: hotspare_add_003_neg
#
# DESCRIPTION: 
# 'zpool add' with hot spares will fail
# while the hot spares belong to the following cases:
#	- nonexistent device,
#	- part of an active pool,
#	- currently mounted,
#	- a swap device,
#	- a dump device,
#	- identical with the basic or spares vdev within the pool,
#	- belong to a exported or potentially active ZFS pool,
#	- a volume device that belong to the given pool,
#
# STRATEGY:
#	1. Create case scenarios
#	2. For each scenario, try to add [-f] the device to the pool
#	3. Verify the add operation failes as expected.
#
# TESTABILITY: explicit
#
# TEST_AUTOMATION_LEVEL: automated
#
# CODING_STATUS: COMPLETED (2006-06-07)
#
# __stc_assertion_end
#
###############################################################################

verify_runnable "global"

function cleanup
{
	poolexists "$TESTPOOL" && \
		destroy_pool "$TESTPOOL"
	poolexists "$TESTPOOL1" && \
		destroy_pool "$TESTPOOL1"

	log_onfail $UMOUNT $TMPDIR/mounted_dir
	log_onfail $SWAPOFF $swap_dev
	log_onfail $DUMPON -r $dump_dev

	partition_cleanup
}

log_assert "'zpool add [-f]' with hot spares should fail with inapplicable scenarios."

log_onexit cleanup

set_devs

mounted_dev=${DISK0}
swap_dev=${DISK1}
dump_dev=${DISK2}
nonexist_dev=${DISK2}bad_slice_num

create_pool "$TESTPOOL" "${pooldevs[0]}"
log_must poolexists "$TESTPOOL"

create_pool "$TESTPOOL1" "${pooldevs[1]}"
log_must poolexists "$TESTPOOL1"

log_must $MKDIR $TMPDIR/mounted_dir
log_must $NEWFS $mounted_dev
log_must $MOUNT $mounted_dev $TMPDIR/mounted_dir

log_must $SWAPON $swap_dev

log_must $DUMPON $dump_dev

#	- nonexistent device,
#	- part of an active pool,
#	- currently mounted,
#	- a swap device,
#	- identical with the basic or spares vdev within the pool,

set -A arg "$nonexist_dev" \
	"${pooldevs[0]}" \
	"${pooldevs[1]}" \
	"$mounted_dev" \
	"$swap_dev"

typeset -i i=0
while (( i < ${#arg[*]} )); do
	if [[ -n "${arg[i]}" ]]; then
		log_mustnot $ZPOOL add $TESTPOOL spare ${arg[i]}
		log_mustnot $ZPOOL add -f $TESTPOOL spare ${arg[i]}
	fi
	(( i = i + 1 ))
done

#	- a dump device,
# https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=241070
# When that bug is fixed, add $dump_dev to $arg and remove this block.
log_must $ZPOOL add $TESTPOOL spare ${dump_dev}
log_must $ZPOOL remove $TESTPOOL ${dump_dev}
log_must $ZPOOL add -f $TESTPOOL spare ${dump_dev}
log_must $ZPOOL remove $TESTPOOL ${dump_dev}

#	- belong to a exported or potentially active ZFS pool,

log_must $ZPOOL export $TESTPOOL1
log_mustnot $ZPOOL add "$TESTPOOL" spare ${pooldevs[1]}
log_must $ZPOOL import -d $HOTSPARE_TMPDIR $TESTPOOL1

log_pass "'zpool add [-f]' with hot spares should fail with inapplicable scenarios."
