#!/bin/sh
#
# $FreeBSD: src/etc/rc.d/rcconf.sh,v 1.4 2005/04/29 23:02:56 brooks Exp $
#

# PROVIDE: rcconf
# BEFORE:  disks initrandom

. /etc/rc.subr

echo "Loading configuration files."
load_rc_config 'XXX'
