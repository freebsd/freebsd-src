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
# ident	"@(#)rootpool_005_pos.ksh	1.1	08/05/14 SMI"
#
#

. $STF_SUITE/include/libtest.kshlib

################################################################################
#
# __stc_assertion_start
#
# ID:  rootpool_005_pos
#
# DESCRIPTION:
#
#  rootpool/ROOT's mountpoint property should be legacy
#
# STRATEGY:
#
# 1) check if the current system is installed as zfs rootfs or not. 
# 2) get the rootpool's name
# 3) get the mountpoint value of rootpool/ROOT
# 4) check to see if the upper value equal to legacy or not.
#
# TESTABILITY: explicit
#
# TEST_AUTOMATION_LEVEL: automated
#
# CODING_STATUS: COMPLETED (2008-02-26)
#
# __stc_assertion_end
#
################################################################################

verify_runnable "global"
log_assert "rootpool/ROOT's mountpoint must be legacy"

typeset rootpool=$(get_rootpool)
typeset mountpoint=$(get_prop mountpoint $rootpool/ROOT)

if [[ $mountpoint != "legacy" ]]; then
	log_note "${rootpool} mountpoint=$mountpoint"
	log_fail "rootpool's mountpoint property is not legacy."
fi

log_pass "rootpool/ROOT's mountpoint is legacy."
