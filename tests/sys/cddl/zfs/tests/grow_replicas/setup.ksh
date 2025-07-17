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

. $STF_SUITE/include/libtest.kshlib

verify_runnable "global"

log_note "Creating pool type: $POOLTYPE"

if [[ -n $DISK ]]; then
	log_note "No spare disks available. Using slices on $DISK"
	partition_disk $SIZE $DISK 4
	create_pool $TESTPOOL $POOLTYPE ${DISK}p1 \
	    ${DISK}p2
else
	wipe_partition_table $DISK0 $DISK1 $DISK2 $DISK3
	log_must set_partition $PARTITION "" $SIZE $DISK0
	log_must set_partition $PARTITION "" $SIZE $DISK1
	log_must set_partition $PARTITION "" $SIZE $DISK2
	log_must set_partition $PARTITION "" $SIZE $DISK3
	create_pool $TESTPOOL $POOLTYPE ${DISK0}p${PARTITION} \
	    ${DISK1}p${PARTITION}
fi

$RM -rf $TESTDIR  || log_unresolved Could not remove $TESTDIR
$MKDIR -p $TESTDIR || log_unresolved Could not create $TESTDIR

log_must $ZFS create $TESTPOOL/$TESTFS
log_must $ZFS set mountpoint=$TESTDIR $TESTPOOL/$TESTFS

log_pass
