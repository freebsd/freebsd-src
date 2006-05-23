#!/bin/sh
#
# $FreeBSD: src/etc/rc.d/early.sh,v 1.1.6.1 2004/10/10 09:50:53 mtm Exp $
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
