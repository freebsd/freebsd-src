#!/bin/sh
#
# $FreeBSD$
#

# PROVIDE: rcconf
# BEFORE:  disks initrandom

. /etc/rc.subr

echo "Loading configuration files."
load_rc_config 'XXX'
