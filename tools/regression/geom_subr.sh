#!/bin/sh
# $FreeBSD: src/tools/regression/geom_subr.sh,v 1.1.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

kldstat -q -m g_${class} || g${class} load || exit 1

devwait()
{
	while :; do
		if [ -c /dev/${class}/${name} ]; then
			return
		fi
		sleep 0.2
	done
}
