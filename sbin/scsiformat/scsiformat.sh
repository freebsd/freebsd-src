#!/bin/sh
#
# Copyright (c) 1995 Peter Dufault
# 
# All rights reserved.
# 
# This program is free software.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 
# THIS SOFTWARE IS PROVIDED BY THE DEVELOPERS ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE DEVELOPERS BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
# 
# $Id: scsiformat.sh,v 1.6 1998/03/01 20:15:00 joerg Exp $
#

PATH="/sbin:/usr/sbin:/bin:/usr/bin"; export PATH

READONLY=yes
DOIT=no
QUIET=no
RAW=
PAGE=0

usage()
{
	echo "usage: scsiformat [-qyw] [-p page-control] raw-device-name" 1>&2
	exit 2
}

while getopts "qwyp:" option
do
	case $option in
	q)
		QUIET=yes
		;;
	y)
		DOIT=yes
		;;
	w)
		READONLY=no
		;;
	p)
		case $OPTARG in
		c)
			PAGE=0
			;;
		d)
			PAGE=2
			;;
		s)
			PAGE=3
			;;
		v)
			PAGE=1
echo "*** note: for variable parameters, 1-bit means 'can write here'"
			;;
		*)
			usage
			;;
		esac
		;;
	?)
		usage
		;;
	esac
done

shift $(($OPTIND - 1))

if [ $# -ne 1 ] ; then
	usage
fi

RAW=$1

if [ "x$RAW" = "x" ] ; then
	usage
fi

if expr "$RAW" : 'sd[0-9][0-9]*$' > /dev/null ; then
	# generic disk name given, convert to control device name
	RAW="/dev/r${RAW}.ctl"
fi

scsi -f $RAW -v -c "12 0 0 0 v 0" 96 -i 96 "s8 z8 z16 z4" || exit $?

if [ "$QUIET" = "no" ] ; then
	scsi -f $RAW \
-v -c "1A 0 v:2 4:6 0 64 0" $PAGE \
-i 72 "{Mode data length} i1 \
{Medium type} i1 \
{Device Specific Parameter} i1 \
{Block descriptor length} i1 \
{Density code} i1 \
{Number of blocks} i3 \
{Reserved} i1 \
{Block length} i3 \
{PS} b1 \
{Reserved} b1 \
{Page code} b6 \
{Page length} i1 \
{Number of Cylinders} i3 \
{Number of Heads} i1 \
{Starting Cylinder-Write Precompensation} i3 \
{Starting Cylinder-Reduced Write Current} i3 \
{Drive Step Rate} i2 \
{Landing Zone Cylinder} i3 \
{Reserved} b6 \
{RPL} b2 \
{Rotational Offset} i1 \
{Reserved} i1 \
{Medium Rotation Rate} i2 \
{Reserved} i1 \
{Reserved} i1 " || exit $?
fi	# !quiet

if [ "$READONLY" = "no" ]
then
	if [ "$DOIT" != "yes" ]
	then
		echo "This will destroy all data on this drive!"
		echo -n "Hit return to continue, or INTR (^C) to abort: "
		read dummy
	fi
	# formatting may take a huge amount of time, set timeout to 4 hours
	echo "Formatting... this may take a while."
	scsi -s 14400 -f $RAW -c "4 0 0 0 0 0"
fi
