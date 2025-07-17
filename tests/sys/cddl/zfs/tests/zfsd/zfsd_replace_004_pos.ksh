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
# Copyright 2023 Axcient.  All rights reserved.
# Use is subject to license terms.
#

. $STF_SUITE/tests/hotspare/hotspare.kshlib
. $STF_SUITE/tests/zfsd/zfsd.kshlib
. $STF_SUITE/include/libtest.kshlib
. $STF_SUITE/include/libgnop.kshlib

log_assert "ZFSD will automatically replace a spare that disappears and reappears in the same location, with the same devname"

ensure_zfsd_running

set_disks

typeset DISK0_NOP=${DISK0}.nop
typeset DISK1_NOP=${DISK1}.nop

log_must create_gnops $DISK0 $DISK1

# Create a pool on the supplied disks
create_pool $TESTPOOL $DISK0_NOP spare $DISK1_NOP

# Disable the first disk.
log_must destroy_gnop $DISK1

# Check to make sure ZFS sees the disk as removed
wait_for_pool_dev_state_change 20 $DISK1_NOP REMOVED

# Re-enable the disk
log_must create_gnop $DISK1

# Disk should auto-join the zpool
wait_for_pool_dev_state_change 20 $DISK1_NOP AVAIL

$ZPOOL status $TESTPOOL
destroy_pool $TESTPOOL
log_must $RM -rf /$TESTPOOL

log_pass
