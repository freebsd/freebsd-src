#!/bin/sh
# $FreeBSD: src/tools/regression/geom_subr.sh,v 1.1.10.1.6.1 2010/12/21 17:09:25 kensmith Exp $

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
