#!/bin/sh
#
# $FreeBSD: src/etc/rc.d/rcconf.sh,v 1.2 2003/01/25 20:02:35 mtm Exp $
#

# PROVIDE: rcconf
# REQUIRE: initdiskless
# BEFORE:  disks initrandom
# KEYWORD: FreeBSD

. /etc/rc.subr

echo "Loading configuration files."
load_rc_config 'XXX'
