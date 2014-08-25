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
# ident	"@(#)r_verify_booting.ksh	1.2	07/01/09 SMI"
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

typeset -i ret

for ds in $RTESTPOOL $RTESTPOOL/$RTESTFS; do
	$ZFS list -H $ds >/dev/null 2>&1
	ret=$?
	(( $ret != 0 )) && _err_exit $ret \
		"Dataset $ds doesn't exit."
done 

mntpt=`$ZFS get -H -o value mountpoint $RTESTPOOL/$RTESTFS`
[[ "$mntpt" != "$RTESTDIR" ]] && _err_exit 1 \
	"The $RTESTPOOL/$RTESTFS doesn't mount successfully."

exit 0
