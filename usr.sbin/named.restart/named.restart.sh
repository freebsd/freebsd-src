#!/bin/sh -
#
#	from named.restart	5.4 (Berkeley) 6/27/89
# $FreeBSD: src/usr.sbin/named.restart/named.restart.sh,v 1.2.2.1 2001/07/19 05:11:06 kris Exp $
#

if [ -r /etc/defaults/rc.conf ]; then
        . /etc/defaults/rc.conf
        source_rc_confs
elif [ -r /etc/rc.conf ]; then
        . /etc/rc.conf
fi
exec %DESTSBIN%/%INDOT%ndc -n ${named_program} restart ${named_flags}
