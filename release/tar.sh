#!/bin/sh -p
#
# Simple replacement for tar(1), using cpio(1).
#
# Copyright (c) 1996 Joerg Wunsch
# All rights reserved.
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

#
# For use on the fixit floppy.  External programs required:
# cpio(1), find(1), test(1)
#

#
# $Id$
#

archive=${TAPE:-/dev/rst0}
blocksize="20"
device=""
mode="none"
verbose=""

usage()
{
	echo "usage: tar -{c|t|x} [-v] [-b blocksize] [-f archive] [files...]" 1>&2
	exit 64		# EX_USAGE
}

#
# Prepend a hyphen to the first arg if necessary, so the traditional form
# ``tar xvf /dev/foobar'' will work, too.  More kludgy legacy forms are not
# supported however.
#

if [ $# -lt 1 ] ; then
	usage
fi

case "$1" in
	-*)	break
		;;
	*)	tmp="$1"
		shift
		set -- -$tmp "$@"
		;;
esac

while getopts "ctxvb:f:" option
do
	case $option in
		[ctx])
			if [ $mode = "none" ] ; then
				mode=$option
			else
				usage
			fi
			;;
		v)
			verbose="-v"
			;;
		b)
			blocksize="${OPTARG}"
			;;
		f)
			archive="${OPTARG## }"
			;;
		*)
			usage
			;;
	esac
done

shift $(($OPTIND - 1))

if [ "X${archive}" != "X-" ] ; then
	device="-F ${archive}"
# else: use stdin or stdout, which is the default for cpio
fi

case $mode in
	none)
		usage
		;;
	t)
		exec cpio -it $verbose $device --block-size="$blocksize" "$@"
		;;
	x)
		exec cpio -idmu $verbose $device --block-size="$blocksize" "$@"
		;;
	c)
		if [ $# -eq 0 ] ; then
			# use current dir -- slightly bogus
			set -- "."
		fi
		find "$@" -print |\
		    cpio -o -H ustar $verbose $device --block-size="$blocksize"
		exit $?
		;;
esac
