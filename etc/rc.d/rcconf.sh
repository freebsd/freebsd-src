#!/bin/sh
#
# $FreeBSD$
#

# PROVIDE: rcconf
# REQUIRE: initdiskless
# BEFORE:  disks initrandom
# KEYWORD: FreeBSD

echo "Loading configuration files."
load_rc_config 'XXX'
