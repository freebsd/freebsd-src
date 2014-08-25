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
RTEST_ROOT=$1
prog=`whence -p $0`
progpath=${prog%/*}
progdirname=${progpath##*/} # test case directory name
commlibpath=${progpath%/*}

. $RTEST_ROOT/stf_config.vars
. $RTEST_ROOT/$progdirname/rebooting.cfg
. $R_PKGDIR/commands.cfg
. $commlibpath/remote_common.kshlib

relpath=${progpath#$R_PKGDIR/}

if [[ "$RDISK" == "detect" ]]; then
	RDISK="$(detectdisks)"
fi

disk=${RDISK%% *}
typeset -i ret

$ZPOOL create $RTESTPOOL $disk
ret=$?
(( $ret != 0 )) && _err_exit $ret \
	"Creating storage pool in $RHOST failed."

$ZFS create $RTESTPOOL/$RTESTFS
ret=$?
(( $ret != 0 )) && _err_exit $ret \
	"Creating zfs filesystem in $RHOST failed."

[[ ! -d $RTESTDIR ]] && $MKDIR -p $RTESTDIR
$ZFS set mountpoint=$RTESTDIR $RTESTPOOL/$RTESTFS
ret=$?
(( $ret != 0 )) && _err_exit $ret \
	"Set mountpoint for $RTESTPOOL/$RTESTFS failed."

exit 0
