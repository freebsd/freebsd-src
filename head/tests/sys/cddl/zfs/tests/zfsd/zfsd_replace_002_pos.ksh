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
# Copyright 2012,2013 Spectra Logic Corporation.  All rights reserved.
# Use is subject to license terms.
# 
# Portions taken from:
# ident	"@(#)replacement_001_pos.ksh	1.4	08/02/27 SMI"
#
# $FreeBSD$

. $STF_SUITE/include/libtest.kshlib
. $STF_SUITE/include/libsas.kshlib

verify_runnable "global"

function cleanup
{
	reap_children
	destroy_pool $TESTPOOL

	# See if the phy has been disabled, and try to re-enable it if possible.
	for CURDISK in $TMPDISKS[*]; do
		if [ ! -z ${EXPANDER_LIST[$CURDISK]} -a ! -z ${PHY_LIST[$CURDISK]} ]; then
			find_disk_by_phy ${EXPANDER_LIST[$CURDISK]} ${PHY_LIST[$CURDISK]}
			[ -n "$FOUNDDISK" ] && continue
		fi
		enable_sas_disk ${EXPANDER_LIST[$CURDISK]} ${PHY_LIST[$CURDISK]}
	done
	rescan_disks

	[[ -e $TESTDIR ]] && log_must $RM -rf $TESTDIR/*
}

log_assert "A pool can come back online after all disks are failed and reactivated"

log_unsupported "This test is currently unsupported, ZFS hangs when all drives fail and come back"

log_onexit cleanup

child_pids=""

ensure_zfsd_running
set -A TMPDISKS $DISKS
NUMDISKS=${#TMPDISKS[*]}

# Trim out any /dev prefix on the disk.
((i=0))   
while [ $i -lt $NUMDISKS ]; do   
	TMPDISKS[$i]=${TMPDISKS[$i]##*/}
	((i++));
done

for type in "raidz" "mirror"; do
	# Create a pool on the supplied disks
	create_pool $TESTPOOL $type $DISKS
	log_must $ZFS create $TESTPOOL/$TESTFS
	log_must $ZFS set mountpoint=$TESTDIR $TESTPOOL/$TESTFS

	unset EXPANDER_LIST
	typeset -A EXPANDER_LIST
	unset PHY_LIST
	typeset -A PHY_LIST

	# First, disable the PHYs for all of the disks.
	for CURDISK in ${TMPDISKS[*]}; do
		# Find the first disk, get the expander and phy
		log_note "Looking for expander and phy information for $CURDISK"
		find_verify_sas_disk $CURDISK

		# Record the expander and PHY for this particular disk, so
		# that we can re-enable the disk later, even if it comes
		# back as a different da(4) instance.
		EXPANDER_LIST[$CURDISK]=$EXPANDER
		PHY_LIST[$CURDISK]=$PHY

		log_note "Disabling \"$CURDISK\" on expander $EXPANDER phy $PHY"
		# Disable the first disk.  We have to do this first, because if
		# there is I/O active to the
		disable_sas_disk $EXPANDER $PHY
	done
	rescan_disks

	# Now go through the list of disks, and make sure they are all gone.
	for CURDISK in ${TMPDISKS[*]}; do
		# Check to make sure disk is gone.
		log_mustnot camcontrol inquiry $CURDISK
	done

	# Make sure that the pool status is "UNAVAIL".  We have taken all
	# of the drives offline, so it should be.
	log_must is_pool_state $TESTPOOL UNAVAIL

	# Now we re-enable all of the PHYs.  Note that we turned off the
	# sleep inside enable_sas_disk, so this should quickly.
	for CURDISK in ${TMPDISKS[*]}; do
		# Re-enable the disk, we don't want to leave it turned off
		log_note "Re-enabling phy ${PHY_LIST[$CURDISK]} on expander ${EXPANDER_LIST[$CURDISK]}"
		enable_sas_disk ${EXPANDER_LIST[$CURDISK]} ${PHY_LIST[$CURDISK]}
	done
	rescan_disks

	unset DISK_FOUND
	typeset -A DISK_FOUND

	log_note "Checking to see whether disks have reappeared"
	((retries=0))
	while [ ${#DISK_FOUND[*]} -lt $NUMDISKS ] && [ $retries -lt 3 ]; do
		# If this isn't the first time through, give the disk a
		# little more time to show up.
		[ $retries -ne 0 ] && $SLEEP 5

		for CURDISK in ${TMPDISKS[*]}; do
			# If we already found this disk, we don't need to
			# check again.  Note that the new name may not be
			# the same as the name referenced in CURDISK.  That
			# is why we look for the disk by expander and PHY.
			[ ! -z ${DISK_FOUND[$CURDISK]} ] && continue

			# Make sure the disk is back in the topology
			find_disk_by_phy ${EXPANDER_LIST[$CURDISK]} ${PHY_LIST[$CURDISK]}
			if [ ! -z "$FOUNDDISK" ]; then
				# This does serve as a mapping from the old
				# disk name to the new disk name.
				DISK_FOUND[$CURDISK]=$FOUNDDISK
			fi
		done
		((retries++))
	done

	if [ ${#DISK_FOUND[*]} -lt $NUMDISKS ]; then
		for CURDISK in ${TMPDISKS[*]}; do
			[ ! -z ${DISK_FOUND[$CURDISK]} ] && continue
			log_note "Disk $CURDISK has not appeared at phy $PHY_LIST[$CURDISK] on expander $EXPANDER_LIST[$CURDISK] after 20 seconds"
		done
		((num_missing=${NUM_DISKS} - ${#DISK_FOUND[*]}))
		log_fail "Missing $num_missing Disks out of $NUM_DISKS Disks"
	else
		for CURDISK in ${TMPDISKS[*]}; do
			log_note "Disk $CURDISK is back as ${DISK_FOUND[$CURDISK]}"
		done
		# Reset our array of disks, because we may have disks that
		# have come back at a different ID.  i.e. da0 may now be da7,
		# and da0 may no longer be a disk that we are authorized to use.
		# This is a more generic problem that we may need to tackle
		# with this test.  We may need to reset the DISKS list itself.
		set -A TMPDISKS ${DISK_FOUND[*]}
	fi

	log_note "Raid type is $type"

	# In theory the pool should be back online.
	$ZPOOL status $TESTPOOL |grep ONLINE > /dev/null
	if [ $? != 0 ]; then
		log_fail "Pool $TESTPOOL is disk $TMPDISK did not automatically join the $TESTPOOL"
	else 
		log_note "After reinsertion, disk is back in pool and online"
	fi

	destroy_pool $TESTPOOL
	log_must $RM -rf /$TESTPOOL
done

log_pass
