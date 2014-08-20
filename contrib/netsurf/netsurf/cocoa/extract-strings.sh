#!/bin/sh

for i in $1/*.xib
do
	xib=`basename "$i"`
	strings="$2/$xib.strings"

	ibtool "$i" --generate-strings-file "$strings"
done


