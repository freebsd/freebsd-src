#!/bin/sh
#
# $FreeBSD: src/etc/rc.d/rcconf.sh,v 1.2.6.1 2004/10/10 09:50:54 mtm Exp $
#

# PROVIDE: rcconf
# REQUIRE: initdiskless
# BEFORE:  disks initrandom

. /etc/rc.subr

echo "Loading configuration files."
load_rc_config 'XXX'
