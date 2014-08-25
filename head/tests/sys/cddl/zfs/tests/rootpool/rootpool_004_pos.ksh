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
# ident	"@(#)rootpool_004_pos.ksh	1.1	08/05/14 SMI"
#
#

. $STF_SUITE/include/libtest.kshlib

################################################################################
#
# __stc_assertion_start
#
# ID:  rootpool_004_pos
#
# DESCRIPTION:
#
#  rootfs's canmount property must be noauto
#
# STRATEGY:
#
# 1) check if the current system is installed as zfs rootfs or not. 
# 2) get the rootfs
# 3) get the canmount value of rootfs
# 4) check to see if the upper value equal to noauto or not.
#
# TESTABILITY: explicit
#
# TEST_AUTOMATION_LEVEL: automated
#
# CODING_STATUS: COMPLETED (2008-01-21)
#
# __stc_assertion_end
#
################################################################################

verify_runnable "global"
log_assert "rootfs's canmount property must be noauto"

typeset rootfs=$(get_rootfs)
typeset can_mount=$(get_prop canmount $rootfs)


if [[ $can_mount != "noauto" ]]; then
	log_note "${rootfs} canmount=$can_mount"
	log_fail "rootfs's canmount is not noauto"
fi

log_pass "rootfs's canmount is noauto."
