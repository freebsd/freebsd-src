#!/bin/sh
#
# remove all the #if's from a source file
#
#	$dotat: things/unifdefall.sh,v 1.8 2002/05/15 10:31:20 fanf Exp $
# $FreeBSD$

set -e

basename=`basename $0`
tmp=`mktemp -d -t $basename` || exit 2

unifdef -s "$@" | sort | uniq > $tmp/ctrl
cpp -dM "$@" | sort |
	sed -Ee 's/^#define[ 	]+(.*[^	 ])[ 	]*$/\1/' > $tmp/hashdefs
sed -Ee 's/^([A-Za-z0-9_]+).*$/\1/' $tmp/hashdefs > $tmp/alldef
comm -23 $tmp/ctrl $tmp/alldef > $tmp/undef
comm -12 $tmp/ctrl $tmp/alldef > $tmp/def

echo unifdef \\ > $tmp/cmd
sed -Ee 's/^(.*)$/-U\1 \\/' $tmp/undef >> $tmp/cmd
while read sym
do	sed -Ee '/^('"$sym"')([(][^)]*[)])?([ 	]+(.*))?$/!d;s//-D\1=\4/' $tmp/hashdefs
done < $tmp/def |
	sed -Ee 's/\\/\\\\/g;s/"/\\"/g;s/^/"/;s/$/" \\/' >> $tmp/cmd
echo '"$@"' >> $tmp/cmd
sh $tmp/cmd "$@"

rm -r $tmp
