#!/bin/sh
# Emulate nroff with groff.

prog="$0"
# Default device.
T=-Tascii
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
