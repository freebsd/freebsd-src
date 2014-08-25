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
# ident	"@(#)rebooting_001_pos.ksh	1.2	07/01/09 SMI"
#

. $STF_SUITE/include/libtest.kshlib

################################################################################
#
# __stc_assertion_start
#
# ID: rebooting_001_pos
#
# DESCRIPTION:
#	Do some I/O work in zfs filesystem in a remote machine, reboot it and  
#	and verify the system boots up fine.
#
# STRATEGY:
#	1. Create lots of empty directories in remote zfs filesystem and unlink
#	   these directories.
#	2. Reboot the system
#	3. Verify the system boots up correctly
#
# TESTABILITY: explicit
#
# TEST_AUTOMATION_LEVEL: automated
#
# CODING_STATUS: COMPLETED (2006-05-26)
#
# __stc_assertion_end
#
################################################################################

verify_runnable "global"

log_assert "Verify a system can be rebooted normally after mkdir/rm operations."  

# Testing in remote hosts.

prog=$(whence -p $0)
progpath=${prog%/*}
progdirname=${progpath##*/} # test case directory name
relpath=${progpath#$STF_SUITE/}
R_PKGDIR=$(get_remote_pkgpath $RHOST)

rsh_status "" $RHOST "$R_PKGDIR/$relpath/r_dir_ops $RTEST_ROOT"
(( $? != 0 )) && \
	log_fail "Creating directories in remote host -- $RHOST failed." 

#Check the remote host boots up or not
! verify_remote $RHOST && \
	log_fail "Remote host $RHOST rebooting timeout."

rsh_status "" $rhost "$R_PKGDIR/$relpath/r_verify_booting $RTEST_ROOT"
(( $? != 0 )) && \
	log_fail "The remote system $RHOST boots up unnormally." 

log_pass "The remote system $RHOST boots up normally as expected."
