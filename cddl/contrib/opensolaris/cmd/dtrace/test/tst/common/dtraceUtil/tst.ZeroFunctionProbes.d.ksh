#!/bin/ksh -p
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
# Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#

#ident	"%Z%%M%	%I%	%E% SMI"

##
#
# ASSERTION:
# The -Z option can be used to permit descriptions that match
# zero probes.
#
# SECTION: dtrace Utility/-Z Option;
# 	dtrace Utility/-f Option
#
##


reader()
{
	while true
	do
		sleep 0.1
		cat /COPYRIGHT > /dev/null
	done
}

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

reader &
child=$!

$dtrace -qZf wassup'{printf("Iamkool");}' \
-qf read'{printf("I am done"); exit(0);}'

status=$?

if [ "$status" -ne 0 ]; then
	echo $tst: dtrace failed
fi

kill $child

exit $status
