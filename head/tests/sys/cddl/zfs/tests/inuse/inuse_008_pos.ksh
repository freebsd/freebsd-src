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
# ident	"@(#)inuse_008_pos.ksh	1.5	09/06/22 SMI"
#

. $STF_SUITE/include/libtest.kshlib

################################################################################
#
# __stc_assertion_start
#
# ID: inuse_008_pos
#
# DESCRIPTION:
# Newfs will interfere with devices and spare devices that are in use 
# by exported pool.
#
# STRATEGY:
# 1. Create a regular|mirror|raidz|raidz2 pool with the given disk
# 2. Export the pool
# 3. Try to newfs against the disk, verify it succeeds as expect.
#
# TESTABILITY: explicit
#
# TEST_AUTOMATION_LEVEL: automated
#
# CODING_STATUS: COMPLETED (2006-07-11)
#
# __stc_assertion_end
#
################################################################################

verify_runnable "global"

function cleanup
{
	poolexists $TESTPOOL1 || $ZPOOL import $TESTPOOL1 >/dev/null 2>&1

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
		$ECHO "y" | $NEWFS $t > /dev/null 2>&1
		(( $? !=0 )) && \
			log_fail "newfs over exported pool "\
				"fails unexpected."
	done
	
	return 0
}

log_assert "Verify newfs over exported pool succeed."

log_onexit cleanup

set -A vdevs "" "mirror" "raidz" "raidz1" "raidz2"

typeset -i i=0

for num in 0 1 2 3 ; do
	eval typeset partition=\${FS_SIDE$num}
	disk=${partition%p*}
	partition=${partition##*p}
	if [[ $WRAPPER == *"smi"* && \
		$disk == ${saved_disk} ]]; then
		cyl=$(get_endslice $disk ${saved_partition})
		log_must set_partition $partition "$cyl" $FS_SIZE $disk
	else
		log_must set_partition $partition "" $FS_SIZE $disk
	fi
	saved_disk=$disk
	saved_partition=$partition
done

while (( i < ${#vdevs[*]} )); do
	if [[ -n $SINGLE_DISK && -n ${vdevs[i]} ]]; then
		(( i = i + 1 ))
		continue
	fi

	create_pool $TESTPOOL1 ${vdevs[i]} $vslices spare $sslices
	log_must $ZPOOL export $TESTPOOL1
	verify_assertion "$rawtargets"
	cleanup_devices $vslices $sslices

	(( i = i + 1 ))
done

log_pass "Newfs over exported pool succeed."
