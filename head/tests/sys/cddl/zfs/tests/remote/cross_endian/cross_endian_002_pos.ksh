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
# ident	"@(#)cross_endian_002_pos.ksh	1.2	07/01/09 SMI"
#

. $STF_SUITE/include/libtest.kshlib
. $STF_SUITE/tests/remote/cross_endian/cross_endian_common.kshlib

################################################################################
#
# __stc_assertion_start
#
# ID: cross_endian_002_pos
#
# DESCRIPTION:
#	ZFS filesystem data can be backuped to remote host with any architecture.
#
# STRATEGY:
#	1. Create a zfs filesystem and populate some data in the filesystem
#	2. Backup the data and restore it in a remote host  
#	3. verify the data integrity
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

function cleanup
{
	snapexists $snap1 && \
		log_must $ZFS destroy $snap1

	for file in `$LS $TESTDIR`; do
		$RM -f $file
	done
}

log_assert "Verify any storage pools can be moved between any architecture \
		systems."
log_onexit cleanup

l_arch=`uname -m`
snap1=$TESTPOOL/$TESTFS@$TESTSNAP

# Setup for testing in local host
log_must $CP $STF_SUITE/bin/`$UNAME -p`/* $TESTDIR
gen_cksum_file $TESTDIR
log_must $ZFS snapshot $snap1

# Testing in remote hosts.
prog=$(whence -p $0)
progpath=${prog%/*}
progdirname=${progpath##*/} # test case directory name
relpath=${progpath#$STF_SUITE/}

for rhost in $TESTHOSTS; do
	R_PKGDIR=$(get_remote_pkgpath $rhost)
	r_arch=`$RSH $rhost uname -m`

	$ZFS send $snap1 | $RSH $rhost $ZFS receive -d $RTESTPOOL
	rsh_status "" $rhost "$R_PKGDIR/$relpath/r_verify_recv $RTEST_ROOT"
	(( $? != 0 )) &&
                log_fail " Contents are sent failed between $l_arch and $r_arch."
        log_note "Contents are sent from $l_arch to $r_arch as expected."
done

log_pass 
