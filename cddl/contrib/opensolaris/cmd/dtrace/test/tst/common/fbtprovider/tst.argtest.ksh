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
# Copyright (c) 2024 Mark Johnston <markj@FreeBSD.org>
#

dtrace=$1

$dtrace -q -s /dev/stdin -c "sysctl debug.dtracetest.fbttest=1" <<__EOF__
fbt:dtrace_test:fbttest:entry
{
    printf("%d %d %d %d %d %d %d %d %d %d\n", args[0], args[1], args[2],
        args[3], args[4], args[5], args[6], args[7], args[8], args[9]);
}
__EOF__
