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
# Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
. $STF_SUITE/tests/hotspare/hotspare.kshlib

################################################################################
#
# __stc_assertion_start
#
# ID: hotspare_detach_005_neg
#
# DESCRIPTION:
#	If a hot spare is only in the list of available hot spares
#	but have NOT been activated,
#	invoke "zpool detach" with this hot spare will fail with
#	a return code of 255 and issue an error message.
#
# STRATEGY:
#	1. Create a storage pool with hot spares
#	2. Do 'zpool detach' with the hot spare device
#	4. Verify the operation fail with return code of 255
#		and issue an error message.
#
# TESTABILITY: explicit
#
# TEST_AUTOMATION_LEVEL: automated
#
# CODING STATUS: COMPLETED (2006-06-07)
#
# __stc_assertion_end
#
###############################################################################

verify_runnable "global"

function cleanup
{
	poolexists $TESTPOOL && \
		destroy_pool $TESTPOOL

	partition_cleanup
}

function verify_assertion # dev
{
	typeset dev=$1

	log_mustnot $ZPOOL detach "$TESTPOOL" $dev
	log_must check_hotspare_state "$TESTPOOL" "$dev" "AVAIL"
}

log_assert "'zpool detach <pool> <vdev>' against a hot spare device that NOT activated should fail and issue an error message."

log_onexit cleanup

set_devs

for keyword in "${keywords[@]}" ; do
	setup_hotspares "$keyword"

	iterate_over_hotspares verify_assertion

	destroy_pool "$TESTPOOL"
done

log_pass "'zpool detach <pool> <vdev>' against a hot spare device that NOT activated fail as expected."
