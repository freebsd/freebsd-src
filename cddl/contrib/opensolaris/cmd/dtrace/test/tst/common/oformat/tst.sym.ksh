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
# Copyright (c) 2023 Domagoj Stolfa
#

bname=`basename $0`
dtraceout=/tmp/dtrace.$bname

script()
{
	$dtrace -o $dtraceout.$1 -x oformat=$1 -s /dev/stdin <<__EOF__
BEGIN
{
        sym(0);
        exit(0);
}
__EOF__
}

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1

script json
jq . $dtraceout.json

if [ $? != 0 ]; then
	echo $bname: failed to produce valid JSON. see $dtraceout.json
	exit 1
fi

script xml
xmllint $dtraceout.xml

if [ $? != 0 ]; then
	echo $bname: failed to produce valid XML. see $dtraceout.xml
	exit 1
fi

rm $dtraceout.json
rm $dtraceout.xml

exit 0
