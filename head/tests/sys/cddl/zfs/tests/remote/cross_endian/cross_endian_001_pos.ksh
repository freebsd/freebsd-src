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
# ident	"@(#)cross_endian_001_pos.ksh	1.2	07/01/09 SMI"
#

. $STF_SUITE/include/libtest.kshlib
. $STF_SUITE/tests/remote/cross_endian/cross_endian_common.kshlib

################################################################################
#
# __stc_assertion_start
#
# ID: cross_endian_001_pos
#
# DESCRIPTION:
#	storage pool can be exported and imported between two any architecture
#	machines.
#
# STRATEGY:
#	1. Create a bunch of block files and then create different types of 
#	  storage pool with these files, populate some data to the storage pool
#	2. Exported the storage pool and rcp the block files to a remote host  
#	3. Imported the pool in the remote host and verify the data integrity
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
	typeset file
	typeset pool
	typeset tmpfile=$TMPDIR/import.list.${TESTCASE_ID}

	$ZPOOL import -d $TESTDIR >$tmpfile
	for pool in $NORMALPOOL $MIRRORPOOL $RAIDZPOOL $RAIDZ2POOL; do
		$GREP $pool $tmpfile >/dev/null 2>&1
		if (( $? == 0 )); then
			log_must $ZPOOL import -d $TESTDIR $pool
			$ZPOOL destroy  -f $pool
		elif poolexists $pool; then
			log_must $ZPOOL destroy -f $pool
		fi
	done

	for file in ${poolfile[*]}; do
		[[ -e $file ]] && $RM -f $file
	done
}

log_assert "Verify any storage pools can be moved between any architecture \
		systems."
log_onexit cleanup

set -A pooltype "regular" "mirror" "raidz" "raidz2"
set -A poolfile "$TESTDIR/$TESTFILE1" "$TESTDIR/$TESTFILE2" \
		"$TESTDIR/$TESTFILE3" "$TESTDIR/$TESTFILE4" \
		"$TESTDIR/$TESTFILE5" "$TESTDIR/$TESTFILE6" \
		"$TESTDIR/$TESTFILE7" "$TESTDIR/$TESTFILE8" 
l_arch=`uname -m`

# Setup for testing in local host

# create bunches of block files to create pools
for file in ${poolfile[*]}; do 
	log_must $MKFILE $FILESZ $file
done
for type in ${pooltype[*]}; do
	case $type in
		"regular")
			log_must $ZPOOL create $NORMALPOOL ${poolfile[0]}
			;;
		"mirror")
			log_must $ZPOOL create $MIRRORPOOL $type ${poolfile[1]} \
				${poolfile[2]}
			;;
		"raidz")
			log_must $ZPOOL create $RAIDZPOOL $type ${poolfile[3]} \
				 ${poolfile[4]}
			;;
		"raidz2")
			log_must $ZPOOL create $RAIDZ2POOL $type ${poolfile[5]}\
				 ${poolfile[6]} ${poolfile[7]}\
			;;
	esac
done

for pool in $NORMALPOOL $MIRRORPOOL $RAIDZPOOL $RAIDZ2POOL; do
	log_must $ZFS create $pool/$RTESTFS
	log_must $CP $STF_SUITE/bin/`$UNAME -p`/* /$pool/$RTESTFS
	gen_cksum_file /$pool/$RTESTFS

	log_must $ZPOOL export $pool
done

# Testing in remote hosts.

prog=$(whence -p $0)
progpath=${prog%/*}
progdirname=${progpath##*/} # test case directory name
relpath=${progpath#$STF_SUITE/}
for rhost in $TESTHOSTS; do
	R_PKGDIR=$(get_remote_pkgpath $rhost)
	r_arch=`$RSH $rhost uname -m`
	for file in ${poolfile[*]}; do
		log_must $RCP -p $file $rhost:$RTEST_ROOT/$progdirname/
	done
	rsh_status "" $rhost "$R_PKGDIR/$relpath/r_verify_import $RTEST_ROOT"
	(( $? != 0 )) && \
		log_fail "Storage pools move failed between $l_arch and $r_arch." 
	log_note "All types of storage pools move from $l_arch to $r_arch \
		as expected." 
done

log_pass 
