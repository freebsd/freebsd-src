#!/bin/sh
#
# $FreeBSD$
#

# PROVIDE: early
# REQUIRE: disks localswap
# BEFORE:  fsck

#
# Support for legacy /etc/rc.early script
#
if [ -r /etc/rc.early ]; then
	warn 'Use of the early.sh script is deprecated'
	warn 'Please use a new-style rc.d script instead'
	warn 'See rc(8) for more information'
	. /etc/rc.early
fi
