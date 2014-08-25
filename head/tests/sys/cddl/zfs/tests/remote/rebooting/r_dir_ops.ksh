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
# ident	"@(#)r_dir_ops.ksh	1.2	07/01/09 SMI"
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

maindirs="1"
subdirs="A B C D E F G H I J K L M N O P"

#
# The following is a common functin to make a large amount of empty 
# directories if the passed argument is '/bin/mkdir', or to remove
# all created empty directories if the passed argument is '/bin/rm -rf'.
# Note: since unlink is not usable in ZFS filesystem, we use 'rm' instead
#       of 'unlink' here.
#
# $1 command used for testing:/bin/mkdir or /bin/rm -rf 
#
function make_rm_dir #<directory operation command>
{
	typeset cmd="$@"
	typeset v0=""
	typeset v1=""
	typeset v2=""
	typeset v3=""
	typeset -i ret

	$CD $RTESTDIR
	for v0 in $maindirs
	do
		if [[ "$cmd" == "$MKDIR" ]]; then
			$cmd $v0
			ret=$?
			(( $ret != 0 )) && _err_exit $ret \
				"$cmd $RTESTDIR/$v0 failed."
		fi
	
		for v1 in $subdirs; do
			for v2 in $subdirs; do
				for v3 in $subdirs; do
					$cmd $v0/$v1$v2$v3
					ret=$?
					(( $ret != 0 )) && _err_exit $ret \
					    "$cmd $RTESTDIR/$v0/$v1$v2$v3 failed."
				done # for v3
			done # for v2
		done # for v1
		if [[ "$cmd" == "$RM -rf" ]]; then
			$cmd $v0
			ret=$?
			(( $ret != 0 )) && _err_exit $ret \
				"$cmd $RTESTDIR/$v0 failed."
		fi
	done # for v0
}

$ZPOOL iostat -v $RTESTPOOL 2 >/dev/null 2>&1 &

$ECHO "Creating directory ..."
make_rm_dir $MKDIR 

$ECHO "Removing directory ..."
make_rm_dir "$RM -rf"

# Verifying the mkdir/rm testing result:
typeset -i dirnum
dirnum=`$FIND $RTESTDIR -type d -print | $WC -l`
(( $dirnum != 1 )) && _err_exit 1 \
	"The directory has not removed cleanly."

$ECHO "Rebooting the system ..."
$REBOOT
