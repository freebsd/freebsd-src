#!/bin/sh
case `file /unix 2>/dev/null` in
*64*)
	bits=64
	;;
*)
	bits=32
	;;
esac

case $1 in
milli)
	if [ $bits = 64 ] ; then
		echo 64
	fi
	;;
*)
	echo $bits
	;;
esac
exit 0
