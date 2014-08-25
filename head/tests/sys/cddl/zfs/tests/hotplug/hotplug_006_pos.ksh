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
# ident	"@(#)hotplug_006_pos.ksh	1.2	08/02/27 SMI"
#

. $STF_SUITE/tests/hotplug/hotplug.kshlib

#################################################################################
#
# __stc_assertion_start
#
# ID: hotplug_006_pos
#
# DESCRIPTION:
#	When unsetting/setting autoreplace, then replacing offlined device,
#	verify device's status is 'UNAVAIL/ONLINE'. No FMA fault is generated.
#
# STRATEGY:
#	1. Create mirror/raidz/raidz2 pool.
#	2. Unsetting/Setting autoreplace
#	3. Synchronise with device in the background.
#	4. Offline one of device, remove it and insert a new device.
#	5. Verify device status's is 'UNAVAIL/ONLINE' with/without FMA fault.
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
	log_must destroy_lofi_device $NEWFILE
}

log_assert "When unsetting/setting autoreplace, then replacing device, verify"\
	"the device's status is 'UNAVAIL/ONLINE'. No FMA fault is generated."
log_onexit cleanup

for type in "mirror" "raidz" "raidz2"; do
	for tab in "off UNAVAIL TRUE" "on ONLINE FALSE"; do
		typeset val=$($ECHO $tab | $AWK '{print $1}')
		typeset status=$($ECHO $tab | $AWK '{print $2}')
		typeset expect=$($ECHO $tab | $AWK '{print $3}')

		log_note "Start $type autoreplace($val) + expect_stat($status)"
		setup_testenv $TESTPOOL $type
		log_must $ZPOOL set autoreplace=$val $TESTPOOL

		typeset file=$(random_get $DEV_FILES)
		typeset device=$(convert_lofi $file)
		log_must $ZPOOL offline $TESTPOOL $device
		log_must remove_device $device
		log_must $ZPOOL clear $TESTPOOL

		# Recreate NEWFILE to avoid dirty data on NEWFILE
		log_must create_file 100M $NEWFILE
		log_must insert_device $NEWFILE $device

		log_must verify_device_status $TESTPOOL $device $status 
		log_must fma_faulty $expect

		cleanup_testenv $TESTPOOL
		log_must remove_device $NEWFILE
	done
done

log_pass "When unsetting/setting autoreplace, then replacing device, verify"\
	"the device's status is 'UNAVAIL/ONLINE'. No FMA fault is generated."
