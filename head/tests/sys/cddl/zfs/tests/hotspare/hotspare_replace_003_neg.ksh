#!/usr/local/bin/ksh93
#
# Copyright (c) 2010 Spectra Logic Corporation
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions, and the following disclaimer,
#    without modification.
# 2. Redistributions in binary form must reproduce at minimum a disclaimer       
#    substantially similar to the "NO WARRANTY" disclaimer below
#    ("Disclaimer") and any redistribution must be conditioned upon
#    including a substantially similar Disclaimer requirement for further
#    binary redistribution.
#
# NO WARRANTY
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL     
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
# STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
# IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGES.
#
# $Id$
# $FreeBSD$

. $STF_SUITE/include/libtest.kshlib
. $STF_SUITE/include/libsas.kshlib

# Reproduction script for Rally DE189:
# https://rally1.rallydev.com/#/9096795496d/detail/defect/13345916506
#
# To reproduce, from Keith's email:
# 1.  Create a 3 drive raid.
# 2.  Disable a drive
# 3.  Let a Spare take over
# 4.  Disable that Spare when it is done rebuilding.
# 5.  Let 2nd spare finish rebuilding
# 6.  Enable the first spare
# 7.  Enable the original drive
# 8.  Destroy the pool.

cleanup() {
	if poolexists $TESTPOOL; then
		# Test failed, provide something useful.
		log_note "For reference, here is the final $TESTPOOL status:"
		zpool status $TESTPOOL
		log_must destroy_pool $TESTPOOL
	fi
	[[ $DISK0_PHY != 0 ]] && enable_sas_disk $DISK0_EXPANDER $DISK0_PHY
	[[ $SPARE0_PHY != 0 ]] && enable_sas_disk $SPARE0_EXPANDER $SPARE0_PHY
	[[ $SPARE1_PHY != 0 ]] && enable_sas_disk $SPARE1_EXPANDER $SPARE1_PHY
}

log_onexit cleanup
trap cleanup TERM INT

typeset -A MEMBERS
typeset -A SPARES
typeset DISK0_EXPANDER=0
typeset DISK0_PHY=0
typeset SPARE0_EXPANDER=0
typeset SPARE0_PHY=0
typeset SPARE1_EXPANDER=0
typeset SPARE1_PHY=0

for disk in $DISKS; do
	if [[ $DISK0_PHY == 0 ]]; then
		find_verify_sas_disk $disk
		DISK0_PHY=$PHY
		DISK0_EXPANDER=$EXPANDER
		DISK0_NAME=$disk
		set -A MEMBERS "${MEMBERS[@]}" $disk
		continue
	fi
	if [[ $SPARE0_PHY == 0 ]]; then
		find_verify_sas_disk $disk
		SPARE0_PHY=$PHY
		SPARE0_EXPANDER=$EXPANDER
		SPARE0_NAME=$disk
		set -A SPARES "${SPARES[@]}" $disk
		continue
	fi
	if [[ $SPARE1_PHY == 0 ]]; then
		find_verify_sas_disk $disk
		SPARE1_PHY=$PHY
		SPARE1_EXPANDER=$EXPANDER
		SPARE1_NAME=$disk
		set -A SPARES "${SPARES[@]}" $disk
		continue
	fi
	# Already filled those positions?  Add disks to the raidz.
	if [[ ${#DISKS[*]} -lt 3 ]]; then
		find_verify_sas_disk $disk
		[[ -z "$DISK1_NAME" ]] && DISK1_NAME=$disk
		DISK1_LONG_NAME=`find_disks ${DISK1_NAME}`
		DISK1_SHORT_NAME=${DISK1_LONG_NAME##/dev/}
		set -A MEMBERS "${MEMBERS[@]}" $disk
		continue
	fi
	break
done

# Remove labels etc. from all the disks we're about to use.
poolexists && log_must destroy_pool $TESTPOOL
cleanup_devices ${MEMBERS[*]} ${SPARES[*]}

log_must $ZPOOL create -f $TESTPOOL raidz1 ${MEMBERS[@]} spare ${SPARES[@]}
DISK0_GUID=$(get_disk_guid $DISK0_NAME)

log_must disable_sas_disk $DISK0_EXPANDER $DISK0_PHY
log_must $ZPOOL replace $TESTPOOL $DISK0_GUID $SPARE0_NAME
wait_until_resilvered
SPARE0_GUID=$(get_disk_guid $SPARE0_NAME)

log_must disable_sas_disk $SPARE0_EXPANDER $SPARE0_PHY
log_must $ZPOOL replace $TESTPOOL $SPARE0_GUID $SPARE1_NAME
wait_until_resilvered

log_must enable_sas_disk $SPARE0_EXPANDER $SPARE0_PHY
log_must enable_sas_disk $DISK0_EXPANDER $DISK0_PHY

log_must destroy_pool $TESTPOOL

# Screen scrape the 'zpool import' output to ensure that the pool doesn't
# show up, since it's been destroyed.
badpoolstate=0
$ZPOOL import | \
	while read word1 word2 rest; do
		echo "$word1 $word2 $rest"
		case "$word1 $word2" in
			"pool: $TESTPOOL") (( badpoolstate += 1 )) ;;
			"$DISK1_LONG_NAME ONLINE") (( badpoolstate += 2 )) ;;
			"$DISK1_SHORT_NAME ONLINE") (( badpoolstate += 2 )) ;;
			*) ;;
		esac
	done

case $badpoolstate in
	0)	log_pass ;;
	1)	log_note "Destroyed pool visible, but probably another pool"
		log_pass ;;
	2)	log_fail "One of our disks is visible but pool is not?!" ;;
	3)	log_fail "Destroyed pool visible!" ;;
	*)	log_fail "Unexpected output.  Update the test" ;;
esac

# Bad output looks like this:
#   pool: testpool.2358
#     id: 5289863802080396071
#  state: UNAVAIL
# status: One or more devices are missing from the system.
# action: The pool cannot be imported. Attach the missing
#    devices and try again.
#   see: http://illumos.org/msg/ZFS-8000-6X
# config:
#
#    testpool.2358              UNAVAIL  missing device
#      raidz1-0                 DEGRADED
#        spare-0                UNAVAIL
#          4564766431272474500  FAULTED  corrupted data
#          1988887717739330825  FAULTED  corrupted data
#        da4                    ONLINE
#        da5                    ONLINE
#    spares
#      1988887717739330825
#      da3
