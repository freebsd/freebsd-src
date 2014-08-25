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
. ${STF_SUITE}/tests/remote/remote_common.kshlib

(( ${#RHOSTS} == 0 )) && log_unsupported

verify_runnable "global"

prog=$(whence -p $0)
progpath=${prog%/*}
progdirname=${progpath##*/} # testcase directory name
relpath=${progpath#$STF_SUITE/} # relative path to suite top directory

rsh_status ""  $RHOST "$MKDIR -m 0777 $RTEST_ROOT/$progdirname"
(( $? != 0 )) && \
       	log_fail "Create directory in remote host failed."

RDISK="$(get_disks $RHOST)"
R_PKGDIR=$(get_remote_pkgpath $RHOST)

#Transfer the parameters to remote host
rcfgfile=$TMPDIR/rebooting.cfg
$ECHO "#!/usr/local/bin/ksh93 -p" >$rcfgfile
for varname in	RTESTPOOL RTESTFS RTESTDIR RHOST RDISK R_PKGDIR
do
	$ECHO "export $varname=\"$(eval $ECHO \${TESTCASE_ID}varname)\"" >>$rcfgfile
done 
log_must $RCP $rcfgfile $RHOST:$RTEST_ROOT/$progdirname/
$RM -f $rcfgfile

rsh_status "" $RHOST "$R_PKGDIR/$relpath/r_setup $RTEST_ROOT " 
(( $? != 0 )) && \
	log_fail "Set up remote system failed."

DISK=${DISKS%% *}
default_setup $DISK

log_pass
