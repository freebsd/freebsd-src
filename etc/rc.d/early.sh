#!/bin/sh
#
# $FreeBSD: src/etc/rc.d/early.sh,v 1.1 2003/04/24 08:27:29 mtm Exp $
#

# PROVIDE: early
# REQUIRE: disks localswap
# BEFORE:  fsck
# KEYWORD: FreeBSD

#
# Support for legacy /etc/rc.early script
#
if [ -r /etc/rc.early ]; then
	. /etc/rc.early
fi
