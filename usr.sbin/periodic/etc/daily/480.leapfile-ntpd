#!/bin/sh
#
#

# If there is a global system configuration file, suck it in.
#
if [ -r /etc/defaults/periodic.conf ]
then
    . /etc/defaults/periodic.conf
    source_periodic_confs
fi

case "$daily_ntpd_leapfile_enable" in
    [Yy][Ee][Ss])
	if service ntpd enabled && service ntpd needfetch; then
	    anticongestion
	    service ntpd fetch
	fi
	;;
esac

exit $rc
