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
# ident	"@(#)cleanup.ksh	1.3	08/12/17 SMI"
#

. ${STF_SUITE}/include/libtest.kshlib

(( ${#RHOSTS} == 0 )) && log_unsupported

verify_runnable "global"

prog=$(whence -p $0)
progpath=${prog%/*}
progdirname=${progpath##*/} # test case directory name
relpath=${progpath#$STF_SUITE/} # relative path to the test suite top directory
# Get the test suite package installation directory in the remote host
R_PKGDIR=$(get_remote_pkgpath $RHOST)

rsh_status "" $RHOST "$R_PKGDIR/$relpath/r_cleanup $RTEST_ROOT"
(( $? != 0 )) && \
	log_fail "Cleanup remote host failed."

default_cleanup
