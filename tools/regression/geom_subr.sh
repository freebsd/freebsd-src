#!/bin/sh
# $FreeBSD: src/tools/regression/geom_subr.sh,v 1.1.10.1.8.1 2012/03/03 06:15:13 kensmith Exp $

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
