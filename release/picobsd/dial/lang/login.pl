#!/bin/sh
# $Id: login.pl,v 1.1.1.1 1998/08/27 17:38:42 abial Exp $

if [ "$2" != "root" ]
then
	exit
fi
cat /etc/motd
LANG=pl; export LANG
HOME=/root exec -sh
