#!/bin/sh
# This file is in the public domain
# $FreeBSD$

set -e 

sh reduce.sh

if [ "x$1" != "x" ] ; then
	OPLIST=$1
else
	OPLIST=no_list
fi

OPLIST=_.options

ODIR=/usr/obj/`pwd`
RDIR=${ODIR}/_.result
export ODIR RDIR

table_td () (

	awk -v R=$1 -v T=$2 -v M=$4 '
	BEGIN	{
		t= R "-" T
	}
	$1 == t {
		if ($3 == 0 && $5 == 0 && $7 == 0) {
			printf "<TD align=center COLSPAN=5>no effect</TD>"
		} else {
			if ($3 == 0) {
				printf "<TD align=right>+%d</TD>", $3
			} else {
				printf "<TD align=right>"
				printf "<A HREF=\"%s/%s.mtree.add.txt\">+%d</A>", M, t, $3
				printf "</TD>"
			}
			if ($5 == 0) {
				printf "<TD align=right>-%d</TD>", $5
			} else {
				printf "<TD align=right>"
				printf "<A HREF=\"%s/%s.mtree.sub.txt\">-%d</A>", M, t, $5
				printf "</TD>"
			}
			if ($7 == 0) {
				printf "<TD align=right>*%d</TD>", $7
			} else {
				printf "<TD align=right>"
				printf "<A HREF=\"%s/%s.mtree.chg.txt\">*%d</A>", M, t, $7
				printf "</TD>"
			}
			printf "<TD align=right>%d</TD>", $9
			printf "<TD align=right>%d</TD>", -$11
		}
		printf "\n"
		d = 1
		}
	END	{
		if (d != 1) {
			printf "<TD COLSPAN=5></TD>"
		}
	}
	' $3/stats
	mkdir -p $HDIR/$4
	cp $3/R*.txt $HDIR/$4 || true
)

HDIR=${ODIR}/HTML
rm -rf ${HDIR}
mkdir -p ${HDIR}
H=${HDIR}/index.html

echo "<HTML>" > $H
echo "<TABLE border=2>" >> $H

echo "<TR>" >> $H
echo "<TH ROWSPAN=3>make.conf</TH>" >> $H
echo "<TH COLSPAN=5>Ref</TH>" >> $H
echo "<TH COLSPAN=5>Ref</TH>" >> $H
echo "<TH COLSPAN=5>Ref</TH>" >> $H
echo "</TR>" >> $H

echo "<TR>" >> $H
echo "<TH COLSPAN=5>BuildWorld</TH>" >> $H
echo "<TH COLSPAN=5>InstallWorld</TH>" >> $H
echo "<TH COLSPAN=5>World</TH>" >> $H
echo "</TR>" >> $H

echo "<TR>" >> $H
for i in bw iw w
do
	echo "<TH>A</TH>" >> $H
	echo "<TH>D</TH>" >> $H
	echo "<TH>C</TH>" >> $H
	echo "<TH>KB</TH>" >> $H
	echo "<TH>Delta</TH>" >> $H
done
echo "</TR>" >> $H

grep -v '^[ 	]*#' $OPLIST | while read o
do
	md=`echo "$o=/dev/YES" | md5`
	m=${RDIR}/$md
	if [ ! -d $m ] ; then
		continue
	fi
	if [ ! -f $m/stats ] ; then
		continue
	fi

	echo "<TR>" >> $H
	echo "<TD><PRE>" >> $H
	cat $m/make.conf >> $H
	echo "</PRE></TD>" >> $H
	if [ -f $m/bw/_.bw ] ; then
		echo "<TD align=center COLSPAN=5>failed</TD>" >> $H
	else
		table_td R BW $m $md >> $H
	fi
	if [ -f $m/iw/_.iw ] ; then
		echo "<TD align=center COLSPAN=5>failed</TD>" >> $H
	else
		table_td R IW $m $md >> $H
	fi
	if [ -f $m/w/_.iw -o -f $m/bw/_.bw ] ; then
		echo "<TD align=center COLSPAN=5>failed</TD>" >> $H
	else
		table_td R W $m $md >> $H
	fi
	echo "</TR>" >> $H
done
echo "</TABLE>" >> $H
echo "</HTML>" >> $H

rsync -r $HDIR/. phk@critter:/tmp/HTML
rsync -r $HDIR/. phk@phk:www/misc/build_options
