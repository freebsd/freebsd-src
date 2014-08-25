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
# ident	"@(#)inuse_005_pos.ksh	1.4	09/06/22 SMI"
#

. $STF_SUITE/include/libtest.kshlib

################################################################################
#
# __stc_assertion_start
#
# ID: inuse_005_pos
#
# DESCRIPTION:
# newfs will not interfere with devices and spare devices that are in use 
# by active pool.
#
# STRATEGY:
# 1. Create a regular|mirror|raidz|raidz2 pool with the given disk
# 2. Try to newfs against the disk, verify it fails as expect.
#
# TESTABILITY: explicit
#
# TEST_AUTOMATION_LEVEL: automated
#
# CODING_STATUS: COMPLETED (2005-12-30)
#
# __stc_assertion_end
#
################################################################################

verify_runnable "global"

function cleanup
{
	poolexists $TESTPOOL1 && destroy_pool $TESTPOOL1

	#
	# Tidy up the disks we used.
	#
	cleanup_devices $vdisks $sdisks
}

function verify_assertion #slices
{
	typeset targets=$1

	for t in $targets; do
		log_mustnot eval "$ECHO y | $NEWFS $t"
	done

	return 0
}

log_assert "Verify newfs over active pool fails."

log_onexit cleanup

set -A vdevs "" "mirror" "raidz" "raidz1" "raidz2"
 
typeset -i i=0

while (( i < ${#vdevs[*]} )); do

	for num in 0 1 2 3 ; do
		eval typeset partition=\${FS_SIDE$num}
		disk=${partition%p*}
		partition=${partition##*p}
		if [[ $WRAPPER == *"smi"* && \
			$disk == ${saved_disk} ]]; then
			cyl=$(get_endslice $disk ${saved_slice})
			log_must set_partition $partition "$cyl" $FS_SIZE $disk
		else
			log_must set_partition $partition "" $FS_SIZE $disk
		fi
		saved_disk=$disk
		saved_slice=$partition
	done

	if [[ -n $SINGLE_DISK && -n ${vdevs[i]} ]]; then
		(( i = i + 1 ))
		continue
	fi

	create_pool $TESTPOOL1 ${vdevs[i]} $vslices spare $sslices
	$ZPOOL status $TESTPOOL1
	log_note "Running newfs on $rawtargets ..."
	verify_assertion "$rawtargets"
	destroy_pool $TESTPOOL1
	wipe_partition_table $vdisks $sdisks

	if [[ ( $FS_DISK0 == $FS_DISK2 ) && -n ${vdevs[i]} ]]; then
		(( i = i + 1 ))
		continue
	fi

	if [[ ( $FS_DISK0 == $FS_DISK3 ) && ( ${vdevs[i]} == "raidz2" ) ]]; then
		(( i = i + 1 ))
		continue
	fi

	create_pool $TESTPOOL1 ${vdevs[i]} $vdisks spare $sdisks
	verify_assertion "$rawtargets"
	destroy_pool $TESTPOOL1

	(( i = i + 1 ))
done

log_pass "Newfs over active pool fails."
