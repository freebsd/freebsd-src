#!/bin/sh -x
# Copyright (c) 2002 Alexey Zelkin <phantom@FreeBSD.org>
#
# ent.sh -- this scripts checks for correcness {authors,teams}.ent files
#
# $FreeBSD$

: ${CVSROOT=/home/ncvs}
prefix=doc/en_US.ISO8859-1/share/sgml
cvs='cvs -Q co -p'
diff='diff -u'
tmp=${TMPDIR-/tmp}/_entities

ckfile() {

ckf=$1

$cvs $prefix/$ckf 2>/dev/null |
	grep ENTITY |
	awk '{ print $2 }' > $tmp.entsrc
sort -u $tmp.entsrc > $tmp.entsrc2
$diff $tmp.entsrc $tmp.entsrc2 > $ckf.order

$cvs $prefix/$ckf 2>/dev/null |
	perl -ne 'print "$1 -- $2\n" if /ENTITY ([^ ]+).*<email>(.*)<\/email>/' |
	grep -vi freebsd.org > $ckf.addr

}

ckresults() {

ckf=$1

if [ -s $ckf.order ]; then
	echo "Ordering check for $ckf failed. See $ckf.ordering file for details."
else
	rm -f $ckf.order
	echo "Ordering check for $ckf is Ok. "
fi

if [ -s $ckf.addr ]; then
	echo "Email addresses for $ckf failed. See $ckf.addr file for details."
else
	rm -f $ckf.addr
	echo "Email addresses check for $ckf is Ok. "
fi

}

ckfile "authors.ent"
ckfile "teams.ent"

echo

ckresults "authors.ent"
ckresults "teams.ent"

rm -f $tmp.entsrc $tmp.entsrc2
