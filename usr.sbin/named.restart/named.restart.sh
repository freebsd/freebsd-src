#!/bin/sh -
#
#	from named.restart	5.4 (Berkeley) 6/27/89
# $FreeBSD: src/usr.sbin/named.restart/named.restart.sh,v 1.4 2001/06/14 04:34:40 dd Exp $
#

if [ -r /etc/defaults/rc.conf ]; then
        . /etc/defaults/rc.conf
        source_rc_confs
elif [ -r /etc/rc.conf ]; then
        . /etc/rc.conf
fi
exec %DESTSBIN%/%INDOT%ndc -n ${named_program} restart ${named_flags}
