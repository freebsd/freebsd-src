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
# ident	"@(#)hotplug_005_pos.ksh	1.2	08/02/27 SMI"
#

. $STF_SUITE/tests/hotplug/hotplug.kshlib

#################################################################################
#
# __stc_assertion_start
#
# ID: hotplug_005_pos
#
# DESCRIPTION:
#	Regarding of autoreplace, when removing offline device and reinserting
#	again. This device's status is 'ONLINE' . No FMA fault was generated.
#
# STRATEGY:
#	1. Create mirror/raidz/raidz2 pool.
#	2. Synchronise with device in the background.
#	3. Offline one of device, remove it and reinsert again.
#	4. Verify device status is 'ONLINE'.
#	5. Verify no FMA faultwas generated.
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

log_assert "Regarding of autoreplace, when removing offline device and " \
	"reinserting again. This device's status is 'ONLINE'. " \
	"No FMA fault was generated."
log_onexit cleanup

for type in "mirror" "raidz" "raidz2"; do
	setup_testenv $TESTPOOL $type
	typeset val=$(random_get "on" "off")
	log_must $ZPOOL set autoreplace=$val $TESTPOOL

	typeset file=$(random_get $DEV_FILES)
	typeset device=$(convert_lofi $file)
	log_must $ZPOOL offline $TESTPOOL $device
	log_must remove_device $device
	log_must $ZPOOL clear $TESTPOOL
	log_must insert_device $file $device

	log_must verify_device_status $TESTPOOL $device 'ONLINE' 
	log_must fma_faulty 'FALSE'

	cleanup_testenv $TESTPOOL
done

log_pass "Regarding of autoreplace, when removing offline device and " \
	"reinserting again. This device's status is 'ONLINE'. " \
	"No FMA fault was generated."
