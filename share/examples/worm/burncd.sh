#!/bin/sh
#
# usage: burncd input-file
#
# Note: This is set up to work ONLY on the HP 4020i CDR drive!
# See the man page for wormcontrol(1) and change the wormcontrol commands
# to match your drive, if the man page lists it as supported.
#
# This script also requires the usage of team(1), an optional component from
# the FreeBSD ports collection. 

if ! pkg_info -e team-3.1; then
	echo "$0: You do not appear to have the team package installed."
	echo
	echo "Please see /usr/ports/misc/team-3.1 if you have the ports"
	echo "collection on your machine, or install the team package from"
	echo "your CD or the net.  To install team from the net right now,"
	echo "simply type:"
	echo
	echo "pkg_add ftp://ftp.freebsd.org/pub/FreeBSD/packages/All/team-3.1.tgz"
	echo
	echo "when logged in (or su'd to) root."
	exit 1
fi

if [ $# -lt 1 ]; then
	echo "usage: $0 input-file"
elif [ "`id -u`" != "0" ]; then
	echo "Sorry, this must be done as root."
else
	echo -n "Place CD in the worm drive now and press return: "
	read junk
	scsi -f /dev/rworm0.ctl -c "0 0 0 0 0 0" >/dev/null 2>&1
	wormcontrol select HP 4020i
	wormcontrol prepdisk double
	wormcontrol track data
	rtprio 5 team -v 1m 5 < $1 | dd of=/dev/rworm0 obs=20k
	wormcontrol fixate 1
fi
