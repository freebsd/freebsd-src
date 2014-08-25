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
# $Id: //SpectraBSD/stable/tests/sys/cddl/zfs/tests/sas_phy_thrash/sas_phy_thrash_001_pos.ksh#2 $
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

export CHILDREN=""
export FAILFILES=""

log_onexit docleanup

typeset i=0
typeset -i num_disks_used=0

for i in $DISKS
do
	# See if this disk is attached to a parent that supports SMP
	# XXX this only works with the current scheme where SMP commands get
	# sent to a device or its parent, if the device doesn't support SMP
	camcontrol smprg $i > /dev/null 2>&1
	if [ $? != 0 ]; then
		continue
	fi

	# Find the expander and PHY that this disk is attached to, if any.
	# We will exit from here if there is a failure.
	find_verify_sas_disk $i

	typeset -i x=0
	log_note "running test on $i on $EXPANDER phy $PHY"
	export FAILFILE=$TMPDIR/${EXPANDER}.${PHY}.failed
	trap diskcleanup INT TERM && rm -f $FAILFILE && while `true`; do
		((x=x+1))
		log_note "attempt number $x on $EXPANDER phy $PHY"
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
		find_disk_by_phy $EXPANDER $PHY
		camcontrol inquiry $FOUNDDISK -v
		if [ $? != 0 ]; then
			log_note "Failed on $EXPANDER phy $PHY attempt $x"
			echo "Expander $EXPANDER phy $PHY failed" >> $FAILFILE
			break
		fi
	done &
	CHILDREN="$CHILDREN $!"
	FAILFILES="$FAILFILES $FAILFILE"
	((num_disks_used++))
done

# The minimum sleep time is 3 minutes for 8 or more disks.  For fewer
# disks, we need to go longer to generate the number of events necessasry
# to trigger the bug.  Scale it up by the number of disks we actually have.
typeset -i sleep_time=$SAS_DEFAULT_TIME

if [ $num_disks_used -lt $SAS_MIN_DEFAULT_DISKS ]; then
	((sleep_time *= (SAS_MIN_DEFAULT_DISKS / num_disks_used)))
fi

# XXX KDM need to stop the entire test as soon as any one of the child
# processes fails.  How does that work in the test framework?
if [ $num_disks_used -gt 0 ]; then
	log_note "Tests queued on $num_disks_used disks"
	log_note "Waiting $sleep_time seconds for potential driver failure"
	disktimeout $sleep_time &
	wait
#	sleep $sleep_time

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
