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
# Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# ident	"@(#)r_cleanup.ksh	1.4	09/05/19 SMI"
#

. ${STF_SUITE}/include/libtest.kshlib

RTEST_ROOT=$1
prog=`whence -p $0`
progpath=${prog%/*}
progdirname=${progpath##*/} # test case directory name
commlibpath=${progpath%/*}
PKGNAME=$(get_package_name)
PKGDIR=`print $(/usr/bin/pkginfo -l $PKGNAME | \
                /usr/bin/grep BASEDIR: | cut -d: -f2)`

. $RTEST_ROOT/stf_config.vars
. $RTEST_ROOT/$progdirname/cross_endian.cfg
. $PKGDIR/commands.cfg
. $commlibpath/remote_common.kshlib

typeset -i ret
for pool in $RTESTPOOL $NORMALPOOL $MIRRORPOOL $RAIDZPOOL $RAIDZ2POOL; do
	$ZPOOL list -H $pool >/dev/null 2>&1
	if (( $? == 0 )); then
		$ZPOOL destroy -f $pool
		ret=$?
		(( $ret != 0 )) && _err_exit $ret \
			"Destroying pool $pool failed."
	fi
done

[[ -d $RTEST_ROOT/$progdirname ]] && $RM -rf $RTEST_ROOT/$progdirname

exit 0

