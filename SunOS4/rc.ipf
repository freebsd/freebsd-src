#!/bin/sh
# $Id$

cd /usr/local/ip_fil || exit 1

umask 022

#SYM='-sym'

if /usr/etc/modstat | grep -s 'IP Filter'; then
	echo ip filter module already loaded
else
	if [ ! -f if_ipl.o ]; then
		echo missing if_ipl.o
		exit 1
	fi
	if modload $SYM if_ipl.o; then
		echo loaded if_ipl
	else
		echo if_ipl load failed
		exit 1
	fi

	echo starting ipmon
	# syslog any logged packets
	/usr/local/bin/ipmon -s &
fi

# allow me to run ipfstat as myself (i'm in group kmem)
chmod 640 /dev/ipl /dev/ipauth /dev/ipnat /dev/ipstate
chgrp kmem /dev/ipl /dev/ipauth /dev/ipnat /dev/ipstate

# create loopback routes for all interface addrs
echo adding loopback routes
./mkroutes

echo loading filters
./reload

# pass reload status:
exit $?
