#!/bin/sh
#
# $FreeBSD$
#

# PROVIDE: rcconf
# REQUIRE: initdiskless
# BEFORE:  disks initrandom
# KEYWORD: FreeBSD

. /etc/rc.subr

echo "Loading configuration files."
load_rc_config 'XXX'
