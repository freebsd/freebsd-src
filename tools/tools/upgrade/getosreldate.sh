#! /bin/sh
# $Id: getosreldate.sh,v 1.1 1998/10/17 05:40:46 peter Exp $

RELDATE=`sysctl -n kern.osreldate 2>/dev/null`
if [ "x$RELDATE" = x ]; then
  RELDATE=200000	# assume something really old
fi
echo $RELDATE
