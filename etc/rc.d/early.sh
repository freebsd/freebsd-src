#!/bin/sh
#
# $FreeBSD: src/etc/rc.d/early.sh,v 1.2.12.1 2008/10/02 02:57:24 kensmith Exp $
#

# PROVIDE: early
# REQUIRE: disks localswap
# BEFORE:  fsck

#
# Support for legacy /etc/rc.early script
#
if [ -r /etc/rc.early ]; then
	. /etc/rc.early
fi
