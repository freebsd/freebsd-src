#!/bin/sh
#       $OpenBSD: scp-ssh-wrapper.sh,v 1.1 2004/06/13 13:51:02 dtucker Exp $
#       Placed in the Public Domain.

printname () {
	NAME=$1
	save_IFS=$IFS
	IFS=/
	set -- `echo "$NAME"`
	IFS="$save_IFS"
	while [ $# -ge 1 ] ; do
		if [ "x$1" != "x" ]; then
			echo "D0755 0 $1"
		fi
		shift;
	done
}

# discard first 5 args
shift; shift; shift; shift; shift

BAD="../../../../../../../../../../../../../${DIR}/dotpathdir"

case "$SCPTESTMODE" in
badserver_0)
	echo "D0755 0 /${DIR}/rootpathdir"
	echo "C755 2 rootpathfile"
	echo "X"
	;;
badserver_1)
	echo "D0755 0 $BAD"
	echo "C755 2 file"
	echo "X"
	;;
badserver_2)
	echo "D0755 0 $BAD"
	echo "C755 2 file"
	echo "X"
	;;
badserver_3)
	printname $BAD
	echo "C755 2 file"
	echo "X"
	;;
badserver_4)
	printname $BAD
	echo "D0755 0 .."
	echo "C755 2 file"
	echo "X"
	;;
*)
	exec $1
	;;
esac
