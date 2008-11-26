#! /bin/sh
# $FreeBSD: src/tools/tools/upgrade/getosreldate.sh,v 1.2.46.1 2008/10/02 02:57:24 kensmith Exp $

RELDATE=`sysctl -n kern.osreldate 2>/dev/null`
if [ "x$RELDATE" = x ]; then
  RELDATE=200000	# assume something really old
fi
echo $RELDATE
