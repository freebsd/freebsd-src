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
# ident	"@(#)hotplug_004_pos.ksh	1.2	08/02/27 SMI"
#

. $STF_SUITE/tests/hotplug/hotplug.kshlib

#################################################################################
#
# __stc_assertion_start
#
# ID: hotplug_004_pos
#
# DESCRIPTION:
#	When device replacement fails, the original device's state will be
#	'UNAVAIL' and an FMA fault will be generated.
#
# STRATEGY:
#	1. Create mirror/raidz/raidz2 pool.
#	2. Synchronise with device in the background.
#	3. Create a small device which is smaller than the minimal ZFS device.
#	4. Replace one device with the small device.
#	5. Verify the device replacement failed
#	6. Verify that an FMA fault was generated
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
	log_must destroy_lofi_device $SMALLFILE
}

log_assert "When device replacement fails, the original device's state will " \
	"be 'UNAVAIL' and an FMA fault will be generated."
log_onexit cleanup

for type in "mirror" "raidz" "raidz2"; do
	setup_testenv $TESTPOOL $type

	typeset file=$(random_get $DEV_FILES)
	typeset device=$(convert_lofi $file)
	log_must remove_device $device
	log_must $ZPOOL clear $TESTPOOL

	# Recreate again to avoid dirty data on device.
	log_must create_file 64M $SMALLFILE
	log_must insert_device $SMALLFILE $device

	log_must verify_device_status $TESTPOOL $device 'UNAVAIL'
	log_must fma_faulty 'TRUE'

	cleanup_testenv $TESTPOOL
	log_must remove_device $SMALLFILE
done

log_pass "When device replacement fails, the original device's state will be " \
	"'UNAVAIL' and an FMA fault will be generated."
