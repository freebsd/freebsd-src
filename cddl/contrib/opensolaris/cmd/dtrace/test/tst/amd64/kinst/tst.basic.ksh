#!/usr/bin/ksh
#
# This file and its contents are supplied under the terms of the
# Common Development and Distribution License ("CDDL"), version 1.0.
# You may only use this file in accordance with the terms of version
# 1.0 of the CDDL.
#
# A full copy of the text of the CDDL should have accompanied this
# source.  A copy of the CDDL is also available via the Internet at
# http://www.illumos.org/license/CDDL.
#

#
# Copyright (c) 2022 Mark Johnston <markj@FreeBSD.org>
#

script()
{
	$dtrace -q -s /dev/stdin <<__EOF__
kinst::vm_fault: {}
kinst::amd64_syscall: {}
kinst::exit1: {}

tick-10s {exit(0);}
__EOF__
}

spin()
{
	while true; do
		ls -la / >/dev/null 2>&1
	done
}

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

spin &
child=$!

script
exit $?
