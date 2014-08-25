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
# Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# ident	"@(#)hotplug_002_pos.ksh	1.2	08/02/27 SMI"
#

. $STF_SUITE/tests/hotplug/hotplug.kshlib

#################################################################################
#
# __stc_assertion_start
#
# ID: hotplug_002_pos
#
# DESCRIPTION:
#	When removing a device from a redundant pool, then reinserting  it 
#	again, this device's state will be indicated as 'ONLINE' regardless of 
#	autoreplace was set or unset. No FMA faulty message.
#
# STRATEGY:
#	1. Create mirror/raidz/raidz2 pool.
#	2. Synchronise with device in the background.
#	3. Set autoreplace is on or off
#	4. Remove device from pool and reinsert again.
#	5. Detect removed devices status is 'ONLINE'.
#	6. Detect no FMA faulty message.
#
# TESTABILITY: explicit
#
# TEST_AUTOMATION_LEVEL: automated
#
# CODING_STATUS: COMPLETED (2007-06-01)
#
# __stc_assertion_end
#
################################################################################

verify_runnable "global"

function cleanup
{
	cleanup_testenv $TESTPOOL
}

log_assert "When removing and reinserting a device, the device status is " \
	"ONLINE with no FMA errors."
log_onexit cleanup

for type in "mirror" "raidz" "raidz2"; do
	setup_testenv $TESTPOOL $type

	typeset val=$(random_get "on" "off")
	log_note "Start $type + autoreplace=$val testing ..."
	log_must $ZPOOL set autoreplace=$val $TESTPOOL

	typeset file=$(random_get $DEV_FILES)
	typeset device=$(convert_lofi $file)
	log_must remove_device $device
	log_must $ZPOOL clear $TESTPOOL
	log_must insert_device $file $device

	log_must verify_device_status $TESTPOOL $device 'ONLINE' 
	log_must fma_faulty 'FALSE'

	cleanup_testenv $TESTPOOL
done

log_pass "When removing and reinserting a device, the device status is " \
	"ONLINE with no FMA errors."
