#!/bin/sh
# This file is in the public domain
# $FreeBSD$

rm -rf HTML
mkdir -p HTML

ref_blk=`awk 'NR == 2 {print $3}' Tmp/Ref/_.df`

echo $ref_blk

H=HTML/index.html
echo "<HTML>" > ${H}

echo "<TABLE border=2>" >> ${H}
echo '

<TR>
<TH COLSPAN=13>Build option survey</TH>
</TR>

<TR>
<TH ROWSPAN=2>make.conf</TH>
<TH COLSPAN=4>InstallWorld</TH>
<TH COLSPAN=4>BuildWorld</TH>
<TH COLSPAN=4>Build + InstallWorld</TH>
</TR>

<TR>
<TH>Blocks</TH>
<TH>Delta</TH>
<TH COLSPAN=2>Files</TH>
<TH>Blocks</TH>
<TH>Delta</TH>
<TH COLSPAN=2>Files</TH>
<TH>Blocks</TH>
<TH>Delta</TH>
<TH COLSPAN=2>Files</TH>
</TR>
' >> $H
echo '<TR><TD><I>[empty]</I></TD>' >> $H
echo "<TD align=right>$ref_blk</TD>" >> $H
echo "<TD></TD><TD></TD><TD></TD>" >> $H
echo "<TD align=right>$ref_blk</TD>" >> $H
echo "<TD></TD><TD></TD><TD></TD>" >> $H
echo "<TD align=right>$ref_blk</TD>" >> $H
echo "<TD></TD><TD></TD><TD></TD>" >> $H
echo "</TR>" >> $H

cat no_list | while read o
do
	m=`echo "$o=YES" | md5`
	echo "<TR>" >> $H
	echo "<TD>" >> $H
	cat Tmp/$m/iw/make.conf >> $H
	echo "</TD>" >> $H
	for d in iw bw w
	do
		if [ ! -d Tmp/$m/$d ] ; then
			echo "<TD>-</TD><TD>-</TD><TD>-</TD><TD>-</TD>" >> $H
			continue
		fi
		if [ ! -f Tmp/$m/$d/_.df ] ; then
			echo "<TD>-</TD><TD>-</TD><TD>-</TD><TD>-</TD>" >> $H
			continue
		fi
		blk=`awk 'NR == 2 {print $3}' Tmp/$m/$d/_.df`
		echo "<TD align=right>$blk</TD>" >> $H
		echo "<TD align=right>`expr $blk - $ref_blk`</TD>" >> $H
		mtree -f Tmp/Ref/_.mtree -f Tmp/$m/$d/_.mtree | 
		     sed  '/^		/d' > HTML/$m.$d.mtree.txt

		sub=`grep -cv '^	' < HTML/$m.$d.mtree.txt`
		add=`grep -c '^	' < HTML/$m.$d.mtree.txt`
		echo "<TD align=right><A href=\"$m.$d.mtree.txt\">+$add</A></TD>" >> $H
		echo "<TD align=right><A href=\"$m.$d.mtree.txt\">-$sub</A></TD>" >> $H

	done
	echo "</TR>" >> $H
done
echo "</TABLE>" >> ${H}
echo "</HTML>" >> ${H}

scp -r HTML phk@critter:/tmp
#scp -r HTML phk@phk:www/misc/kernel_options
