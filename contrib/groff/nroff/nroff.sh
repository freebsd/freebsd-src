#!/bin/sh
# Emulate nroff with groff.

prog="$0"
# Default device.
if test `expr "$LC_CTYPE" : ".*\.ISO_8859-1"` -gt 0 || \
   test `expr "$LANG" : ".*\.ISO_8859-1"` -gt 0
then
	T=-Tlatin1
else
if test `expr "$LC_CTYPE" : ".*\.KOI8-R"` -gt 0 || \
   test `expr "$LANG" : ".*\.KOI8-R"` -gt 0
then
	T=-Tkoi8-r
else
	T=-Tascii
fi
fi
opts=

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
	-i|-S|-[mrno]*)
		opts="$opts $1";
		;;

	-Tascii|-Tlatin1|-Tkoi8-r)
		T=$1
		;;
	-T*)
		# ignore other devices
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
exec groff -S -Wall -mtty-char $T $opts ${1+"$@"}
