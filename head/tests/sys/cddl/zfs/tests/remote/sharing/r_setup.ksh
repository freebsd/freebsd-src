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
# ident	"@(#)r_setup.ksh	1.2	07/01/09 SMI"
#
export RTEST_ROOT=$1
prog=`whence -p $0`
progpath=${prog%/*}
progdirname=${progpath##*/} # test case directory name
commlibpath=${progpath%/*}

. $RTEST_ROOT/stf_config.vars
. $RTEST_ROOT/$progdirname/r_sharing.cfg
. $R_PKGDIR/commands.cfg
. $commlibpath/remote_common.kshlib

relpath=${progpath#$R_PKGDIR/} # relative path to the test 
				# suite top directory

if [[ "$RDISK" == "detect" ]]; then
	RDISK="$(detectdisks)"
fi
RDISK=${RDISK%% *}

typeset -i ret
$ZPOOL list -H $RTESTPOOL >/dev/null 2>&1
(( $? == 0 )) && \
	$ZPOOL destroy -f $RTESTPOOL

$ZPOOL create -f $RTESTPOOL $RDISK
ret=$?
(( $ret != 0 )) && _err_exit $ret \
	"Creating pool $RTESTPOOL with disk(s) $RDISK failed."

for op_objs in "$RTESTFS $RTESTDIR $SNAP" \
		"$RTESTFS1 $RTESTDIR1 $SNAP1" \
		"$RTESTFS2 $RTESTDIR2 $SNAP2"; do 
	fs=`$ECHO $op_objs | $AWK '{print $1}'`
	ds=$RTESTPOOL/$fs
	mntpt=`$ECHO $op_objs | $AWK '{print $2}'`
	snap=`$ECHO $op_objs | $AWK '{print $3}'`

	$ZFS create $ds
	ret=$?
	(( $ret != 0 )) && _err_exit $ret \
		"Create filesystem $ds failed."

	[[ ! -d $mntpt ]] && $MKDIR -p $mntpt
	$CHMOD 0777 $mntpt
	$ZFS set mountpoint=$mntpt $ds
	ret=$?
	(( $ret != 0 )) && _err_exit $ret \
		"Set mountpont for $ds failed."

	#propagate some data into the filesystem
	$CP -rp $R_PKGDIR/bin/* $mntpt
	$ZFS snapshot $ds@$snap
	ret=$?
	(( $ret != 0 )) && _err_exit $ret \
		"Creatint snapshot $ds@$snap failed."
	$ZFS set sharenfs=$SHROPT $ds
	ret=$?
	(( $ret != 0 )) && _err_exit $ret \
		"Set sharenfs=$SHROPT for $ds failed."

	$SHARE | $GREP $mntpt >/dev/null 2>&1
	if (( $? != 0 )); then 
		$ZFS share $ds
		ret=$?
		(( $ret != 0 )) && _err_exit $ret \
			"Sharing zfs filesystem $ds failed."
	fi
done

exit 0
