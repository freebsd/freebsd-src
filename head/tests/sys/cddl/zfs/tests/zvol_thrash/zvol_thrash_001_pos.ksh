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
# $Id: //depot/SpectraBSD/stable/9/cddl/tools/regression/stc/src/suites/fs/zfs/tests/functional/sas_phy_thrash/sas_phy_thrash_001_pos.ksh#1 $
# $FreeBSD$
#

. $STF_SUITE/include/libtest.kshlib
. $STF_SUITE/include/libsas.kshlib


typeset -i NUM_FAILURES=0
export NUM_FAILURES

# Cleanup function.  Kill each of the children, they will re-enable the PHY
# they're working on.
function docleanup
{
	for CPID in $CHILDREN
	do
		echo "Killing $CPID"
		kill $CPID
	done
	for CPID in $CHILDREN
	do
		wait $CPID
	done
}

# If we get killed, try to re-enable the PHY we were toggling.
function diskcleanup
{
	log_note "Got a signal, sending linkreset to $EXPANDER phy $PHY"
	camcontrol smppc $EXPANDER -o linkreset -p $PHY
	exit 0
}

# Wait for the timeout, and then kill the child processes.
function disktimeout
{
	log_note "disktimeout process waiting $1 seconds"
	sleep $1
	docleanup
}

function mk_vols
{
	ADISKS=($DISKS)				#Create an array for convenience
	N_DISKS=${#ADISKS[@]}
	if test $N_DISKS -ge 8
	then
		#Limit number of mirrors to 4.  Using more causes a panic in
		#make_dev_credv that has nothing to do with ZFS or ZVols.
		#That will be addressed by a separate test
		N_MIRRORS=4
	else
		N_MIRRORS=$(($N_DISKS / 2 ))
	fi
	setup_mirrors $N_MIRRORS $DISKS
	for pool in `all_pools`; do
		# Create 4 ZVols per pool.  Write a geom label to each, just so
		# that we have another geom class between zvol and the vdev
		# taster.  That thwarts detection of zvols based on a geom
		# producer's class name, as was attempted by change 538882
		for ((j=0; $j<4; j=$j+1)); do
			$ZFS create -V 10G $pool/testvol.$j
			glabel label testlabel$j /dev/zvol/$pool/testvol.$j
		done
	done
}

export CHILDREN=""
export FAILFILES=""
export POOLS=""

log_onexit docleanup

typeset i=0
typeset -i num_disks_used=0

log_assert "Cause frequent device removal and arrival in the prescence of" \
    " zvols.  ZFS should not taste them for VDev GUIDs.  If it does," \
    " deadlocks (SpectraLogic MST 23637) and panics (SpectraLogic BUG23665" \
    " and BUG23677) may result"
mk_vols
for p in `all_pools`
do
	disk=`get_disklist $p | cut -d " " -f 1`  #Take the first disk in the pool
	# See if this disk is attached to a parent that supports SMP
	# XXX this only works with the current scheme where SMP commands get
	# sent to a device or its parent, if the device doesn't support SMP
	camcontrol smprg $disk > /dev/null 2>&1
	if [ $? != 0 ]; then
		continue
	fi

	# Find the expander and PHY that this disk is attached to, if any.
	# We will exit from here if there is a failure.
	find_verify_sas_disk $disk

	typeset -i x=0
	log_note "thrashing phy on $disk on $EXPANDER phy $PHY"
	export FAILFILE=$TMPDIR/${EXPANDER}.${PHY}.failed
	trap diskcleanup INT TERM && rm -f $FAILFILE && while `true`; do
		((x=x+1))
		camcontrol smppc $EXPANDER -v -o disable -p $PHY
		if [ $? != 0 ]; then
			log_note "Failed to disable $EXPANDER phy $PHY"
			echo "Expander $EXPANDER phy $PHY failed" >> $FAILFILE
			break
		fi
		$SLEEP 10
		camcontrol smppc $EXPANDER -v -o linkreset -p $PHY
		if [ $? != 0 ]; then
			log_note "Failed to reset $EXPANDER phy $PHY"
			echo "Expander $EXPANDER phy $PHY failed" >> $FAILFILE
			break
		fi
		$SLEEP 10
	done &
	CHILDREN="$CHILDREN $!"
	FAILFILES="$FAILFILES $FAILFILE"
	((num_disks_used++))
done

typeset -i sleep_time=$SAS_DEFAULT_TIME

if [ $num_disks_used -gt 0 ]; then
	log_note "Tests queued on $num_disks_used disks"
	log_note "Waiting $sleep_time seconds for potential driver failure"
	disktimeout $sleep_time &
	wait

	for i in $FAILFILES; do
		typeset FILEBASE=${i%%.failed}
		FILEBASE=${FILEBASE##$TMPDIR/}
		if [ -f $i ]; then
			log_note "Test of $FILEBASE failed"
			((NUM_FAILURES=NUM_FAILURES+1))
			rm -f $i
		else
			log_note "Test of $FILEBASE passed"
		fi
	done
	if [ $NUM_FAILURES -gt 0 ]; then
		log_fail "Saw $NUM_FAILURES failures"
	else
		log_note "Number of failures: $NUM_FAILURES"
		log_pass
	fi
else
	log_unsupported "No tests queued, no SMP-capable devices found"
fi
