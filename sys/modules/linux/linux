#!/bin/sh

# $Id: linux,v 1.4 1997/02/22 12:48:25 peter Exp $

if modstat -n linux_mod > /dev/null ; then
	echo Linux lkm already loaded
	exit 1
else
	modload -e linux_mod -u -q -o /tmp/linux_mod /lkm/linux_mod.o
fi
