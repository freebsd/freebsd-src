#!/bin/sh
# $FreeBSD$

if [ "$2" != "root" ]
then
	exit
fi
cat /etc/motd
LANG=pl; export LANG
HOME=/root exec -sh
