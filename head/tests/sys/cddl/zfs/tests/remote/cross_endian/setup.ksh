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
# ident	"@(#)setup.ksh	1.2	07/01/09 SMI"
#

. ${STF_SUITE}/include/libtest.kshlib

(( ${#RHOSTS} == 0 )) && log_unsupported

verify_runnable "global"

prog=$(whence -p $0)
progpath=${prog%/*}
export progpath
progdirname=${progpath##*/} # testcase directory name
export progdirname
relpath=${progpath#$STF_SUITE/} # relative path to suite top directory

for rhost in $TESTHOSTS; do
	rsh_status ""  $rhost "$MKDIR -m 0777 $RTEST_ROOT/$progdirname"
	(( $? != 0 )) && \
        	log_fail "Create directory in remote host failed."

	#Transfer the parameters to remote host
	rcfgfile=$TMPDIR/cross_endian.cfg
	$ECHO "#!/usr/local/bin/ksh93 -p" >$rcfgfile
	for varname in TESTFILE1 TESTFILE2 TESTFILE3 \
			TESTFILE4 TESTFILE5 TESTFILE6 \
			TESTFILE7 TESTFILE8 NORMALPOOL \
			MIRRORPOOL RAIDZPOOL RAIDZ2POOL \
			RTESTPOOL RTESTFS RTESTFILE \
			TESTFS TESTSNAP;
	do
		$ECHO "export $varname=\"$(eval $ECHO \$$varname)\"" >>$rcfgfile
	done 
	log_must $RCP $rcfgfile $rhost:$RTEST_ROOT/$progdirname
	$RM -f $rcfgfile

	#Initially, create a storage pool for zfs send/recv testing.
	poolfile=$RTEST_ROOT/$progdirname/$RTESTFILE
	rsh_status "" $rhost "$MKFILE $FILESZ $poolfile"
	(( $? != 0 )) && \
		log_fail "Creating $FILESZ file in $rhost failed."
	rsh_status "" $rhost "$ZPOOL create $RTESTPOOL $poolfile" 
	(( $? != 0 )) && \
		log_fail "Creating storage pool in $rhost failed."
done

DISK=${DISKS%% *}
default_setup $DISK

log_pass
