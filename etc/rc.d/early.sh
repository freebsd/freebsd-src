#!/bin/sh
#
# $FreeBSD: src/etc/rc.d/early.sh,v 1.3.10.1 2010/02/10 00:26:20 kensmith Exp $
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
