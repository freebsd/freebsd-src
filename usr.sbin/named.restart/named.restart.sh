#!/bin/sh -
#
#	from named.restart	5.4 (Berkeley) 6/27/89
# $FreeBSD$
#

if [ -r /etc/defaults/rc.conf ]; then
        . /etc/defaults/rc.conf
        source_rc_confs
elif [ -r /etc/rc.conf ]; then
        . /etc/rc.conf
fi
exec %DESTSBIN%/%INDOT%ndc restart ${named_flags}
