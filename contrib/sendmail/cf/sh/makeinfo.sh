#!/bin/sh
#
# Copyright (c) 1998 Sendmail, Inc.  All rights reserved.
# Copyright (c) 1983 Eric P. Allman.  All rights reserved.
# Copyright (c) 1988, 1993
#	The Regents of the University of California.  All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#
#	@(#)makeinfo.sh	8.11 (Berkeley) 5/19/98
#

usewhoami=0
usehostname=0
for p in `echo $PATH | sed 's/:/ /g'`
do
	if [ "x$p" = "x" ]
	then
		p="."
	fi
	if [ -f $p/whoami ]
	then
		usewhoami=1
		if [ $usehostname -ne 0 ]
		then
			break;
		fi
	fi
	if [ -f $p/hostname ]
	then
		usehostname=1
		if [ $usewhoami -ne 0 ]
		then
			break;
		fi
	fi
done
if [ $usewhoami -ne 0 ]
then
	user=`whoami`
else
	user=$LOGNAME
fi

if [ $usehostname -ne 0 ]
then
	host=`hostname`
else
	host=`uname -n`
fi
echo '#####' built by $user@$host on `date`
echo '#####' in `pwd` | sed 's/\/tmp_mnt//'
echo '#####' using $1 as configuration include directory | sed 's/\/tmp_mnt//'
echo "define(\`__HOST__', $host)dnl"
