#!/bin/sh
# $Id: login.pl,v 1.2 1998/08/31 13:36:43 abial Exp $

if [ "$2" != "root" ]
then
	exit
fi
cat /etc/motd
LANG=pl; export LANG
HOME=/root exec -sh
