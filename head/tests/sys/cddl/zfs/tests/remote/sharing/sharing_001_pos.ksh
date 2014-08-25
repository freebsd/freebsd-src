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
# ident	"@(#)sharing_001_pos.ksh	1.2	07/01/09 SMI"
#

. $STF_SUITE/include/libtest.kshlib

################################################################################
#
# __stc_assertion_start
#
# ID: sharing_001_pos
#
# DESCRIPTION:
#	Verify .zfs support with NFS version 3 & 4, but not support with NFS 
#	version 2.
#
# STRATEGY:
#	1. Create three zfs filesystems in remote host, populate them with 
#	   snapshot, and share them   
#	2. Mount the shared filesystems from remote host to local host with 
#	   with different NFS version: 2, 3 and  4
#	3. Verify the data in the file system and the snapshot directory
#
# TESTABILITY: explicit
#
# TEST_AUTOMATION_LEVEL: automated
#
# CODING_STATUS: COMPLETED (2006-04-26)
#
# __stc_assertion_end
#
################################################################################

verify_runnable "global"

function cleanup
{
	typeset mntpt

	for mntpt in $NFSMNTPT $NFSMNTPT1 $NFSMNTPT2; do
		ismounted $mntpt "nfs" && \
			log_must $UMOUNT -f $mntpt
		[[ -d $mntpt ]] && \
			log_must $RM -rf $mntpt
	done
}

#
# Verify the mounting result through different NFS versions
#
# $1 filesystem mountpoint
# $2 snapshot name
# $3 NFS mount version
#

function verify_mounting # <mountpoint> <snapshot> <NFS version>
{
	typeset mntpt=$1	
	typeset snap=$2
	typeset ver=$3
	typeset snapdir=$mntpt/$(get_snapdir_name)/$snap
	typeset snapfile
	typeset fname
	typeset origcksum
	typeset snapcksum
	
	if [[ "$ver" == "2" ]]; then
		[[ -d $snapdir ]] && \
		    log_fail ".zfs should not support via NFS verision $ver"
	else
		for file in `$FIND $mntpt -type f`; do
			fname=${file##$mntpt}
			fname=${fname#/}	
			snapfile=$snapdir/$fname

			[[ ! -e $snapfile ]] && \
				log_fail "The file exists in filesystem" \
				    "but not exists in its snapshot directroy" \
				    "when mounting with NFS version $ver."

			$DIFF $file $snapfile >/dev/null 2>&1
			(( $? != 0 )) && \
				log_fail "The contents of $file differ with its" \
				    "snapshot file $snapfile when mounting with" \
				    "NFS version $ver."

			origcksum="`$CKSUM $file | $AWK '{print $1 $2}'`"
			snapcksum="`$CKSUM $snapfile | $AWK '{print $1 $2}'`"
			[[ "$origcksum" != "$snapcksum" ]] && \
			    log_fail "The checksum of $file differs with its" \
			        "snapshot file $snapfile when mounting with" \
			        "NFS version $ver."		
		done
	fi
}

log_assert "Verify .zfs support for NFS version 3 & 4, but not for version 2."

log_onexit cleanup

for mntpt in $NFSMNTPT $NFSMNTPT1 $NFSMNTPT2; do
	[[ ! -d $mntpt ]] &&  \
	    log_must $MKDIR -p -m 0777 $mntpt
done

for op_objs in "2 $NFSMNTPT $RTESTDIR $SNAP" \
		"3 $NFSMNTPT1 $RTESTDIR1 $SNAP1" \
		"4 $NFSMNTPT2 $RTESTDIR2 $SNAP2"; do
	ver=`$ECHO  $op_objs | $AWK '{print $1}'`
	mntpt=`$ECHO $op_objs | $AWK '{print $2}'`
	shdir=`$ECHO $op_objs | $AWK '{print $3}'`
	snap=`$ECHO $op_objs | $AWK '{print $4}'`
	
	log_must $MOUNT -F nfs -o "rw,vers=$ver" \
			$RHOST:$shdir $mntpt

	verify_mounting $mntpt $snap $ver
done

log_pass
