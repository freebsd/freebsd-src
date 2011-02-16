#!/bin/sh
#
# $FreeBSD: src/etc/rc.d/early.sh,v 1.3.12.1 2010/12/21 17:10:29 kensmith Exp $
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
