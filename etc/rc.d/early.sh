#!/bin/sh
#
# $FreeBSD: src/etc/rc.d/early.sh,v 1.2 2004/10/07 13:55:25 mtm Exp $
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
