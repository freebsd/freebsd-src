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
# Copyright (c) 2011, Joyent, Inc. All rights reserved.
#

ppriv -s A=basic,dtrace_user $$

#
# We expect at least one of these tick probes to error out because only
# dtrace_user is set, and we are attempting to access arguments.  Note that
# this test will fail if something is stuck on CPU that whomever is running
# the test happens to own.
#
/usr/sbin/dtrace -q -s /dev/stdin <<EOF
BEGIN
{
	start = timestamp;
}

tick-1000hz
{
	@[arg0] = count();
}

ERROR
{
	errcnt++;
}

tick-10ms
{
	ticks++;
}

tick-10ms
/ticks > 100/
{
	printf("error count is %d\n", errcnt);
	exit(errcnt != 0 ? 0 : 1);
}
EOF
