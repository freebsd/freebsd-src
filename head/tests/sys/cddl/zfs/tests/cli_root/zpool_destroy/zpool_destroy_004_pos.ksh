#!/usr/local/bin/ksh93 -p
#
# Copyright 2015 Spectra Logic Corporation.
#

. $STF_SUITE/include/libtest.kshlib

################################################################################
#
# __stc_assertion_start
#
# ID: zpool_destroy_004_pos
#
# DESCRIPTION: 
#	'zpool destroy -f <pool>' can forcibly destroy the specified pool,
#       even if that pool has running zfs send or receive activity.
#
# STRATEGY:
#	1. Create a storage pool
#       2. For each sleep time in a set:
#       2a. For each destroy type (same pool, sender only, receiver only):
#	    - Create a dataset with some amount of data
#           - Run zfs send | zfs receive in the background.
#           - Sleep the amount of time specified for this run.
#	    - 'zpool destroy -f' the pool.
#	    - Wait for the send|receive to exit.  It must not be killed in
#	      order to ensure that the destroy takes care of doing so.
#	    - Verify the pool destroyed successfully
#
# __stc_assertion_end
#
###############################################################################

verify_runnable "global"

function cleanup
{
	poolexists $TESTPOOL && destroy_pool $TESTPOOL
	poolexists $TESTPOOL1 && destroy_pool $TESTPOOL1
}

function create_sender
{
	cleanup
	create_pool "$TESTPOOL" "$DISK0"
	log_must $ZFS create $TESTPOOL/$TESTFS
	log_must $MKDIR -p $TESTDIR
	log_must $ZFS set mountpoint=$TESTDIR $TESTPOOL/$TESTFS
	log_must dd if=/dev/zero of=$TESTDIR/f0 bs=1024k count=$datasz
	log_must $ZFS snapshot $TESTPOOL/$TESTFS@snap1
}

function create_sender_and_receiver
{
	create_sender
	create_pool "$TESTPOOL1" "$DISK1"
}

function check_recv_status
{
	rcv_pid="$1"
	rcv_status="$2"
	destroyed="$3"

	if ((rcv_status != 0))
	then
		log_note \
	"zfs receive interrupted by destruction of $destroyed as expected"
		return 0
	fi

	log_note "zfs receive NOT interrupted by destruction of $destroyed"
	case $destroyed
	in
		SAME_POOL|RECEIVER)
		log_fail "expected zfs receive failure did not occur"
		;;
		
		SENDER)
		log_note "zfs send completed before destruction of $destroyed"
		;;

		*)
		log_fail "unknown parameter $destroyed"
		;;
	esac
	return 0
}

function send_recv_destroy
{
	sleeptime=$1
	recv=$2
	to_destroy=$3
	who_to_destroy="$4"

	# The pid of this send/receive pipeline is that of zfs receive.
	# We can not get the exit status of the zfs send. We can, however,
	# infer the status of the send; see note below.
	#
	( $ZFS send -RP $TESTPOOL/$TESTFS@snap1 | $ZFS receive -Fu $recv/d1 ) &
	recvpid=$!
	sndrcv_start=$(date '+%s')

	log_must sleep $sleeptime
	destroy_start=$(date '+%s')
	log_must $ZPOOL destroy -f $to_destroy
	destroy_end=$(date '+%s')
	dtime=$((destroy_end - destroy_start))
	log_note "Destroy of $who_to_destroy took ${dtime} seconds."

	# NOTE:
	# If the we have destroyed the send pool then the send/receive may
	# succeed.
	# 	1.) If the destruction of the send pool interrupts the zfs 
	#	    send then this error will be detected by the receiver;
	#	    the entire operation will fail.
	#
	#	2.) If the send completes before the destruction of the send 
	# 	    pool then the receive will succeed.
	#
	wait $recvpid
	rc=$?
	wait_end=$(date '+%s')
	log_note "$recvpid rc = $rc for destruction of $who_to_destroy"
	check_recv_status $recvpid $rc $who_to_destroy

	wtime=$((wait_end - sndrcv_start))
	log_note "send|receive took ${wtime} seconds to finish."
	log_mustnot $ZPOOL list $to_destroy
}

function run_tests
{
	log_note "TEST: send|receive to the same pool"
	create_sender
	send_recv_destroy $sleeptime $TESTPOOL $TESTPOOL SAME_POOL 

	log_note "TEST: send|receive to different pools, destroy sender"
	create_sender_and_receiver
	send_recv_destroy $sleeptime $TESTPOOL1 $TESTPOOL SENDER

	log_note "TEST: send|receive to different pools, destroy receiver"
	create_sender_and_receiver
	send_recv_destroy $sleeptime $TESTPOOL1 $TESTPOOL1 RECEIVER
}

log_assert "'zpool destroy -f <pool>' can force destroy active pool"
log_onexit cleanup
set_disks

# Faster tests using 1GB data size
datasz=1000
log_note "Running fast tests with 1000MB of data"
for sleeptime in 0.1 0.3 0.5 0.75 1 2 3; do
	run_tests
done

# A longer test that simulates a more realistic send|receive that exceeds
# the size of arc memory by 1/3 and gets interrupted a decent amount of
# time after the start of the run.
arcmem=$(sysctl -n vfs.zfs.arc_max)
# ARC will use 2xdatasz memory since it caches both the src and dst copies
datasz=$((arcmem / 1048576 * 2 / 3))
log_note "Running longer test with ${datasz}MB of data"
sleeptime=15
run_tests

log_pass "'zpool destroy -f <pool>' successful with active pools."
