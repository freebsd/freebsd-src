#!/bin/sh
# $FreeBSD: src/tools/regression/geom_subr.sh,v 1.1 2005/12/07 01:20:18 pjd Exp $

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
