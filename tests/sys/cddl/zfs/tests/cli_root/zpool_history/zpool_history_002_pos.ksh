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

. $STF_SUITE/include/libtest.kshlib

#################################################################################
#
# __stc_assertion_start
#
# ID: zpool_history_002_pos
#
# DESCRIPTION:
#	Verify zpool history can handle options [-il] correctly.
#
# STRATEGY:
#	1. Create varied combinations of option -i & -l.
#	2. Verify 'zpool history' can cope with these combination correctly.
#
# TESTABILITY: explicit
#
# TEST_AUTOMATION_LEVEL: automated
#
# CODING_STATUS: COMPLETED (2006-11-24)
#
# __stc_assertion_end
#
################################################################################

verify_runnable "global"

$ZFS 2>&1 | $GREP "allow" > /dev/null
(($? != 0)) && log_unsupported

log_assert "Verify zpool history can handle options [-il] correctly."

options="-i -l -il -li -lil -ili -lli -iill -liil"

for opt in $options; do
	log_must eval "$ZPOOL history $opt $TESTPOOL > /dev/null 2>&1"
done

log_pass "Verify zpool history can handle options [-il] passed."
