#!/bin/sh
ls $1.* | wc | awk '{ print "Pieces = ",$1 }'
for FILE in $1.*; do \
PIECE=`echo $FILE | cut -d . -f 2` ; \
echo -n "cksum.$PIECE = "; \
cksum $FILE | awk ' { print $1,$2 } '
done
