#!/bin/sh
# This file is in the public domain
# $FreeBSD$

if [ "x$1" != "x" ] ; then
	OPLIST=$1
else
	OPLIST=no_list
fi

OPLIST=_.options

ODIR=/usr/obj/`pwd`
RDIR=${ODIR}/_.result
export ODIR RDIR


compa ( ) (
	if [ ! -f $1/_.mtree ] ; then
		return
	fi
	if [ ! -f $2/_.mtree ] ; then
		return
	fi
	
	mtree -k uid,gid,mode,nlink,size,link,type,flags \
	    -f ${1}/_.mtree -f $2/_.mtree > $2/_.mtree.all.txt
	grep '^		' $2/_.mtree.all.txt > $4/$3.mtree.chg.txt
	grep '^[^	]' $2/_.mtree.all.txt > $4/$3.mtree.sub.txt
	grep '^	[^	]' $2/_.mtree.all.txt > $4/$3.mtree.add.txt
	a=`wc -l < $4/$3.mtree.add.txt`
	s=`wc -l < $4/$3.mtree.sub.txt`
	c=`wc -l < $4/$3.mtree.chg.txt`
	c=`expr $c / 2`

	br=`awk 'NR == 2 {print $3}' $1/_.df`
	bt=`awk 'NR == 2 {print $3}' $2/_.df`
	echo $3 A $a S $s C $c B $bt D `expr $br - $bt`
)

grep -v '^[ 	]*#' $OPLIST | while read o
do
	md=`echo "$o=/dev/YES" | md5`
	m=${RDIR}/$md
	if [ ! -d $m ] ; then
		continue
	fi
	if [ ! -d $m/iw -a ! -d $m/bw -a ! -d $m/w ] ; then
		continue
	fi

	echo
	echo -------------------------------------------------------------
	echo $md
	cat $m/make.conf
	echo -------------------------------------------------------------
	if [ -f $m/iw/done -a ! -f $m/iw/_.mtree ] ; then
		echo "IW failed"
	fi
	if [ -f $m/bw/done -a ! -f $m/bw/_.mtree ] ; then
		echo "BW failed"
	fi
	if [ -f $m/w/done -a ! -f $m/w/_.mtree ] ; then
		echo "W failed"
	fi
	(
	compa ${RDIR}/Ref/ $m/iw R-IW $m
	compa ${RDIR}/Ref/ $m/bw R-BW $m
	compa ${RDIR}/Ref/ $m/w R-W $m
	compa $m/iw $m/w IW-W $m
	compa $m/bw $m/w BW-W $m
	compa $m/bw $m/iw BW-IW $m
	) > $m/stats
	cat $m/stats
done
