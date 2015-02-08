#!/usr/bin/awk
/^[^#]/ {
	gsub(/^\./,"", $1)
	tags=$NF
	gsub(/tags=/,"", tags)
	output=tags".plist"
	uname=$3
	gname=$4
	mode=$5
	gsub(/uname=/, "", uname);
	gsub(/gname=/, "", gname);
	gsub(/mode=/, "", mode);

	print "@("uname","gname","mode") " $1 > output
}
