#!/bin/sh
# $Id: login.pl,v 1.2 1998/07/15 20:11:44 abial Exp $

if [ "$2" != "root" ]
then
	exit
fi
cat /etc/motd
LANG=pl; export LANG
exec -sh
