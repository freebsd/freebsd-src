#!/bin/sh -
#
# Copyright (c) 1987 Regents of the University of California.
# All rights reserved.
#
# Redistribution and use in source and binary forms are permitted
# provided that this notice is preserved and that due credit is given
# to the University of California at Berkeley. The name of the University
# may not be used to endorse or promote products derived from this
# software without specific prior written permission. This software
# is provided ``as is'' without express or implied warranty.
#
#	@(#)newvers.sh	4.4 (Berkeley) 3/28/88
#
if [ ! -r version ]
then
	echo 0 > version
fi
touch version
rm -f version.[oc]
v=`cat version` u=${USER-root} d=`pwd` h=`hostname` t=`date`
sed -e "s|%VERSION%|#${v}: ${t}|" -e "s|%WHOANDWHERE%|${u}@${h}:${d}|" \
	< $1 >version.c
/bin/echo `expr ${v} + 1` > version
