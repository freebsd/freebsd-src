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
# We expect tick probes to fire if dtrace_user is set
#
/usr/sbin/dtrace -q -s /dev/stdin <<EOF
BEGIN
{
	start = timestamp;
}

tick-10ms
{
	ticks++;
}

tick-10ms
/ticks > 10 && (this->ms = (timestamp - start) / 1000000) > 2000/
{
	printf("expected completion in 100 ms, found %d!\n", this->ms);
	exit(1);
}

tick-10ms
/ticks > 10/
{
	printf("completed in %d ms\n", this->ms);
	exit(0);
}
EOF
