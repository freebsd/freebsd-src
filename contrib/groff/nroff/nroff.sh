#!/bin/sh
# Emulate nroff with groff.
# $FreeBSD: src/contrib/groff/nroff/nroff.sh,v 1.8 2000/01/12 10:26:23 asmodai Exp $

prog="$0"
# Default device.
locale=${LC_CTYPE:-$LANG}
if test `expr "$locale" : ".*\.ISO_8859-1"` -gt 0
then
	T=-Tlatin1
else
if test `expr "$locale" : ".*\.KOI8-R"` -gt 0
then
	T=-Tkoi8-r
else
	T=-Tascii
fi
fi
opts=
safer=-S

for i
do
	case $1 in
	-h)
		opts="$opts -P-h"
		;;
	-[eq]|-s*)
		# ignore these options
		;;
	-[mrnoT])
		echo "$prog: option $1 requires an argument" >&2
		exit 1
		;;
	-[iStp]|-[mrno]*)
		opts="$opts $1";
		;;

	-Tascii|-Tlatin1|-Tkoi8-r)
		T=$1
		;;
	-T*)
		# ignore other devices
		;;
	-S)
		# safer behaviour
		safer=-S
		;;
	-U)
		# unsafe behaviour
		safer=-U
		;;
	-u*)
		# Solaris 2.2 `man' uses -u0; ignore it,
		# since `less' and `more' can use the emboldening info.
		;;
	--)
		shift
		break
		;;
	-)
		break
		;;
	-*)
		echo "$prog: invalid option $1" >&2
		exit 1
		;;
	*)
		break
		;;
	esac
	shift
done

# This shell script is intended for use with man, so warnings are
# probably not wanted.  Also load nroff-style character definitions.
exec groff $safer -Wall -mtty-char $T $opts ${1+"$@"}
