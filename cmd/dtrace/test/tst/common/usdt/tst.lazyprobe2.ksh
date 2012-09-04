#!/usr/bin/ksh
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

#
# Copyright (c) 2012 by Delphix. All rights reserved.
#

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

#
# Verify that USDT objects built with -xlazyload fire by default when using
# the DTrace audit library.
#

LD_AUDIT_32=/usr/lib/dtrace/libdtrace_forceload.so ./tst.lazyprobe.exe &
id=$!

ret=1

$dtrace -Z -s /dev/stdin <<-EOF
	lazyprobe*:::fire
	{
		exit(0);
	}
	tick-10hz
	/i++ > 20/
	{
		exit(1);
	}
EOF
ret=$?

kill -9 $id

exit $ret
