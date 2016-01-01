#!/bin/sh
# $FreeBSD$

if [ $(id -u) -ne 0 ]; then
	echo 'Tests must be run as root'
	echo 'Bail out!'
	exit 1
fi
kldstat -q -m g_${class} || geom ${class} load || exit 1

devwait()
{
	while :; do
		if [ -c /dev/${class}/${name} ]; then
			return
		fi
		sleep 0.2
	done
}
