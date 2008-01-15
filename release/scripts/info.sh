#!/bin/sh
#
# $ANA: info.sh,v 1.3 1996/06/04 16:25:30 wollman Exp $
# $FreeBSD: src/release/scripts/info.sh,v 1.5 2001/04/08 23:09:21 obrien Exp $
#

ls $1.[a-z][a-z] | wc | awk '{ print "Pieces = ",$1 }'
for FILE in $1.[a-z][a-z]; do
       PIECE=`echo $FILE | cut -d . -f 2`
       echo -n "cksum.$PIECE = "
       cksum $FILE | awk ' { print $1,$2 } '
done
