#! /bin/sh
# $FreeBSD: src/tools/tools/upgrade/getosreldate.sh,v 1.1.2.1 1999/08/29 15:18:34 peter Exp $

RELDATE=`sysctl -n kern.osreldate 2>/dev/null`
if [ "x$RELDATE" = x ]; then
  RELDATE=200000	# assume something really old
fi
echo $RELDATE
