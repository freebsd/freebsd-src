#!/bin/sh
# $FreeBSD: src/release/picobsd/dial/lang/login.pl,v 1.2.2.1 1999/08/29 15:52:38 peter Exp $

if [ "$2" != "root" ]
then
	exit
fi
cat /etc/motd
LANG=pl; export LANG
HOME=/root exec -sh
