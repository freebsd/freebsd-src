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
# ident	"@(#)hotplug_007_pos.ksh	1.2	08/02/27 SMI"
#

. $STF_SUITE/tests/hotplug/hotplug.kshlib

#################################################################################
#
# __stc_assertion_start
#
# ID: hotplug_007_pos
#
# DESCRIPTION:
#	When autoreplace is 'on', replacing the device with a smaller one.
#	Verify the device's status is 'UNAVAIL'. FMA fault has been generated.
#
# STRATEGY:
#	1. Create mirror/raidz/raidz2 pool.
#	2. Set autoreplace = on
#	3. Synchronise with device in the background.
#	4. Offline and remove one of device, insert a new device.
#	5. Verify the device's status is 'UNAVAIL'.
#	6. Verify FMA fault has been generated.
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

log_unsupported "WARNING: Disable it temporarily due to bug 6563887"

verify_runnable "global"

function cleanup
{
	cleanup_testenv $TESTPOOL
	log_must destroy_lofi_device $SMALLFILE
}

log_assert "When autoreplace is 'on', replacing the device with a smaller one."\
	"Verify the device's status is 'UNAVAIL'. FMA fault has been generated."
log_onexit cleanup

for type in "mirror" "raidz" "raidz2"; do
	setup_testenv $TESTPOOL $type
	log_must $ZPOOL set autoreplace=on $TESTPOOL

	typeset file=$(random_get $DEV_FILES)
	typeset device=$(convert_lofi $file)
	log_must $ZPOOL offline $TESTPOOL $device
	log_must remove_device $device
	log_must $ZPOOL clear $TESTPOOL
	
	# Recreate SMALLFILE to avoid dirty data in SMALLFILE
	log_must create_file 64M $SMALLFILE
	log_must insert_device $SMALLFILE $device

	log_must verify_device_status $TESTPOOL $device 'UNAVAIL'
	log_must fma_faulty 'TRUE'

	cleanup_testenv $TESTPOOL
	log_must remove_device $SMALLFILE
done

log_pass "When autoreplace is 'on', replacing the device with a smaller one."\
	"Verify the device's status is 'UNAVAIL'. FMA fault has been generated."
