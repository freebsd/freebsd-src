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
# We expect some number of these profile probes to be silently dropped.
# Note that this test will fail if something is stuck on all CPUs that
# whomever is running the test happens to own.
#
count=$(/usr/sbin/dtrace -q -s /dev/stdin <<EOF
BEGIN
{
	start = timestamp;
	@ = count();
}

ERROR
{
	exit(1);
}

profile-1000hz
{
	@ = count();
}

tick-10ms
{
	ticks++;
}

tick-10ms
/ticks > 100/
{
	printa("%@d", @);
	exit(0);
}
EOF)

cpus=`psrinfo | grep -- on-line | wc -l`
max=`expr $cpus \* 500`

if [[ $count -gt $max ]]; then
	echo "count ($count) is greater than allowed max ($max)"
	exit 1
fi

echo "count ($count) is within allowed max ($max)"
exit 0
