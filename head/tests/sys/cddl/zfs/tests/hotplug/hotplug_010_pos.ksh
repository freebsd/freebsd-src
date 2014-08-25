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
# Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# ident	"@(#)hotplug_010_pos.ksh	1.1	07/07/31 SMI"
#

. $STF_SUITE/tests/hotplug/hotplug.kshlib

#################################################################################
#
# __stc_assertion_start
#
# ID: hotplug_010_pos
#
# DESCRIPTION:
#	Removing device offlined and reinsert onlined, verify device status is 
#	'ONLINE'.
#
# STRATEGY:
#	1. Create mirror/raidz/raidz2 pool w/a hot spare device.
#	2. Synchronise with device in the background.
#	3. Set or unset autoreplace
#	4. Unmount all filesystems and disable syseventd and fmd.
#	5. Unload ZFS module and remove a device.
#	6. Load ZFS module and insert the device again.
#	7. Verify device's status is 'ONLINE'. No FMA fault is generated.
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

fstype=$(get_fstype '/')
if [[ $fstype == 'zfs' ]]; then
	log_unsupported "This test cases is not supported on ZFS root system."
fi

function cleanup
{
	typeset fmri
	for fmri in 'sysevent' 'fmd'; do
		typeset stat=$($SVCS -H -o STATE $fmri)
		if [[ $stat != 'online' ]]; then
			log_must $SVCADM enable $fmri
		fi
		$SLEEP 5
	done

	cleanup_testenv $TESTPOOL
}

log_assert "Removing device offlined and reinserting onlined, " \
	"verify the device status ONLINE."
log_onexit cleanup

for type in "mirror" "raidz" "raidz2"; do
	setup_testenv $TESTPOOL $type
	typeset val=$(random_get 'off' 'on')
	log_must $ZPOOL set autoreplace=$val $TESTPOOL

	#
	# Piror to unmount, stop background writing to avoid mount failed
	# due to mountpoint is not empty
	#
	log_must kill_bg_write

	#
	# unloading ZFS module to simulate system powered off
	#
	log_must unload_zfs

	typeset file=$(random_get $DEV_FILES)
	typeset device=$(convert_lofi $file)
	log_must remove_device $device

	# Reload ZFS module and check device status
	log_must load_zfs

	# After mount, restart background writing process
	log_must start_bg_write $TESTPOOL

	log_must insert_device $file $device
	log_must verify_device_status $TESTPOOL $device 'ONLINE'
	log_must fma_faulty 'FALSE'

	cleanup_testenv $TESTPOOL
done

log_pass "Removing device offlined and reinserting onlined, " \
	"verify thhe device status ONLINE."
