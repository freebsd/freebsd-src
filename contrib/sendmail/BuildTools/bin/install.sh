#!/bin/sh

# Copyright (c) 1998 Sendmail, Inc.  All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#
#	@(#)install.sh	8.9 (Berkeley) 5/19/1998

# Set default program
program=mv

# chown program -- ultrix keeps it in /etc/chown and /usr/etc/chown
if [ -f /etc/chown ]
then
	chown=/etc/chown
elif [ -f /usr/etc/chown ]
then
	chown=/usr/etc/chown
else
	chown=chown
fi

# Check arguments
while [ ! -z "$1" ]
do
	case $1
	in
	  -o)	owner=$2
		shift; shift
		;;

	  -g)	group=$2
		shift; shift
		;;

	  -m)	mode=$2
		shift; shift
		;;

	  -c)	program=cp
		shift
		;;

	  -s)	strip="strip"
		shift
		;;

	  -*)	echo $0: Unknown option $1
		exit 1
		;;

	  *)	break
		;;
	esac
done

# Check source file
if [ -z "$1" ]
then
	echo "Source file required" >&2
	exit 1
elif [ -f $1 -o $1 = /dev/null ]
then
	src=$1
else
	echo "Source file must be a regular file or /dev/null" >&2
	exit 1
fi

# Check destination
if [ -z "$2" ]
then
	echo "Destination required" >&2
	exit 1
elif [ -d $2 ]
then
	dst=$2/$src
else
	dst=$2
fi

# Do install operation
$program $src $dst
if [ $? != 0 ]
then
	exit 1
fi

# Strip if requested
if [ ! -z "$strip" ]
then
	$strip $dst
fi

# Change owner if requested
if [ ! -z "$owner" ]
then
	$chown $owner $dst
	if [ $? != 0 ]
	then
		exit 1
	fi
fi

# Change group if requested
if [ ! -z "$group" ]
then
	chgrp $group $dst
	if [ $? != 0 ]
	then
		exit 1
	fi
fi

# Change mode if requested
if [ ! -z "$mode" ]
then
	chmod $mode $dst
	if [ $? != 0 ]
	then
		exit 1
	fi
fi

exit 0
