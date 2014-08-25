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
# ident	"@(#)hotplug_003_pos.ksh	1.2	08/02/27 SMI"
#

. $STF_SUITE/tests/hotplug/hotplug.kshlib

#################################################################################
#
# __stc_assertion_start
#
# ID: hotplug_003_pos
#
# DESCRIPTION:
#	Set/Unset autoreplace, remove device from redundant pool and insert new
#	device, this new device state will be indicated as 'ONLINE/UNAVAIL'. 
#	No FMA faulty message.
#
# STRATEGY:
#	1. Create mirror/raidz/raidz2 pool.
#	2. Synchronise with device in the background.
#	3. Set autoreplace=on
#	4. Remove device from pool and insert a new device.
#	5. Verify the new devices status is 'ONLINE'.
#	6. Verify that no FMA faults have been generated.
#	7. Set autoreplace=off, redo steps 4 - 6, verify the new device's
#	   status is 'UNAVAIL'. There are FMA faulty.
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

log_assert "Having removed a device from a redundant pool and inserted a new " \
	"device, the new device state will be 'ONLINE' when autoreplace is on,"\
	"and 'UNAVAIL' when autoreplace is off"
log_onexit cleanup

for type in "mirror" "raidz" "raidz2"; do
	for tab in "off UNAVAIL TRUE" "on ONLINE FALSE"; do
		typeset val=$($ECHO $tab | $AWK '{print $1}')
		typeset status=$($ECHO $tab | $AWK '{print $2}')
		typeset expect=$($ECHO $tab | $AWK '{print $3}')

		log_note "Start $type autoreplace($val) + expect_stat($status)"
		setup_testenv $TESTPOOL $type

		log_must $ZPOOL set autoreplace=$val $TESTPOOL

		# Remove and insert new device $NEWFILE
		typeset file=$(random_get $DEV_FILES)
		typeset device=$(convert_lofi $file)
		log_must remove_device $device
		log_must $ZPOOL clear $TESTPOOL

		# Recreate again to avoid dirty data on device
		log_must create_file 100M $NEWFILE
		log_must insert_device $NEWFILE $device

		log_must verify_device_status $TESTPOOL $device $status 
		log_must fma_faulty $expect

		cleanup_testenv $TESTPOOL
		log_must remove_device $NEWFILE
	done
done

log_pass "Having removed a device from a redundant pool and inserted a new " \
	"device, the new device state will be 'ONLINE' when autoreplace is on,"\
	"and 'UNAVAIL' when autoreplace is off"
