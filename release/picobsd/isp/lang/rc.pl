#!/bin/sh
# $Id: rc.pl,v 1.3 1998/08/10 19:16:43 abial Exp $
############################################
### Special setup for one floppy PICOBSD ###
### THIS IS NOT THE NORMAL /etc/rc !!!!! ###
############################################
mount -a -t nonfs
if [ -f /etc/rc.conf ]; then
    . /etc/rc.conf
fi
rm -f /var/run/*
if [ "x$swapfile" != "xNO" -a -w "$swapfile" -a -b /dev/vn0b ]; then
	echo "Dodaje $swapfile jako dodatkowy swap."
	vnconfig /dev/vn0b $swapfile && swapon /dev/vn0b
fi
# configure serial devices
if [ -f /etc/rc.serial ]; then
	. /etc/rc.serial
fi
# start up the initial network configuration.
if [ -f /etc/rc.network ]; then
	. /etc/rc.network
	network_pass1
fi
mount -a -t nfs
# clean up left-over files
(cd /var/run && { cp /dev/null utmp; chmod 644 utmp; })
if [ -n "$network_pass1_done" ]; then
    network_pass2
fi
if [ -n "$network_pass2_done" ]; then
    network_pass3
fi
if [ "X${inetd_enable}" = X"YES" ]; then
	echo "Uruchamiam inetd."; inetd ${inetd_flags}
fi
if [ "X${snmpd_enable}" = X"YES" ]; then
	echo "Uruchamiam snmpd."; snmpd ${snmpd_flags}
fi

dev_mkdb

echo ''
if [ "x$swapfile" = "xNO" ]; then
	echo "UWAGA: brak swapu!"
	echo "Nie uruchamiaj zbyt wielu programow na raz..."
fi
echo ''
echo ''
echo '+------------ PicoBSD 0.4 (ISP) ---------------+'
echo '|                                              |'
echo '| Zaloguj sie jako "root" (haslo "setup").     |'
echo '|                                              |'
echo '| Ta wersja PicoBSD w pelni podlega            |'
echo '| licencji BSD. Po wiecej szczegolow zajrzyj   |'
echo '| na http://www.freebsd.org/~picobsd, lub      |'
echo '| skontaktuj sie z autorem.                    |'
echo '|                                              |'
echo '|                     abial@nask.pl            |'
echo '|                                              |'
echo '+----------------------------------------------+'
exit 0
