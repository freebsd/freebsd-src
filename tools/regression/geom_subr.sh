#!/bin/sh
# $FreeBSD: src/tools/regression/geom_subr.sh,v 1.1.10.1.2.1 2009/10/25 01:10:29 kensmith Exp $

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
