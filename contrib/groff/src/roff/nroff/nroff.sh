#!/bin/sh
# Emulate nroff with groff.

prog="$0"
# Default device.
if test "X$LC_CTYPE" = "Xiso_8859_1" || test "X$LESSCHARSET" = "Xlatin1"
then
	T=-Tlatin1
else
	T=-Tascii
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
	-i|-[mrno]*)
		opts="$opts $1";
		;;

	-Tascii|-Tlatin1)
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
exec groff -Wall -mtty-char $T $opts ${1+"$@"}
