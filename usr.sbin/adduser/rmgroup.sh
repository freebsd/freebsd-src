#!/bin/sh
# Copyright (c) 1996 Wolfram Schneider <wosch@FreeBSD.org>. Berlin.
# All rights reserved.
#
# rmgroup - delete a Unix group
#
# $Id: rmgroup.sh,v 1.1 1996/10/30 20:41:17 wosch Exp wosch $

PATH=/bin:/usr/bin; export PATH
db=/etc/group

case "$1" in
	""|-*)	echo "usage: rmgroup group"; exit 1;;
	wheel|daemon|kmem|sys|tty|operator|bin|nogroup|nobody)
		echo "Do not remove system group: $1"; exit 2;;
	*) group="$1";;
esac

if egrep -q -- "^$group:" $db; then
	if egrep -q -- "^$group:\*:0:" $db; then
		echo "Do not remove group with gid 0: $group"
		exit 2
	fi
	egrep -v -- "^$group:" $db > $db.new &&
		cp -pf $db $db.bak &&
		mv -f  $db.new $db
else 
	echo "Group \"$group\" does not exists in $db."; exit 1
fi
