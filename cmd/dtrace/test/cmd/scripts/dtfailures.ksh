#!/usr/bin/ksh -p
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

let failure=0

printf "%-3s %-10s %-31s %s\n" "#" "KIND" "TEST" "DETAILS" 

while [[ -d failure.$failure ]]; do
	dir=failure.$failure
	tst=`cat $dir/README | head -1 | nawk '{ print $2 }'`
	kind=`basename $(dirname $tst)`
	name=`basename $tst`
	cols=$(expr `tput cols` - 47)
	details=`tail -1 $dir/*.err | cut -c1-$cols`
	printf "%-3d %-10s %-31s " $failure $kind $name
	echo $details
	let failure=failure+1
done

