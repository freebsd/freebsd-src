#!/bin/sh
# $FreeBSD: src/tools/regression/usr.bin/pkill/pgrep-S.t,v 1.1.20.1 2009/04/15 03:14:26 kensmith Exp $

base=`basename $0`

echo "1..2"

name="pgrep -S"
pid=`pgrep -Sx g_event`
if [ "$pid" = "2" ]; then
	echo "ok 1 - $name"
else
	echo "not ok 1 - $name"
fi
pid=`pgrep -x g_event`
if [ "$pid" != "2" ]; then
	echo "ok 2 - $name"
else
	echo "not ok 2 - $name"
fi
