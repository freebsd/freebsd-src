#!/usr/bin/awk
/^[^#]/ {
	gsub(/^\./,"", $1)
	uname = gname = mode = flags = tags = ""
	for (i=2; i<=NF; i++) {
		if ($i ~ /^uname=/) {
			uname=$i
			gsub(/uname=/, "", uname)
		} else if ($i ~ /^gname=/) {
			gname=$i
			gsub(/gname=/, "", gname)
		} else if ($i ~ /^mode=/) {
			mode=$i
			gsub(/mode=/,"", mode)
		} else if ($i ~ /^flags=/) {
			flags=$i
			gsub(/flags=/, "", flags)
		} else if ($i ~ /^tags=/) {
			tags=$i
			gsub(/tags=/, "", tags)
		}
	}
	if (length(tags) == 0)
		next
	if (tags ~ /package=/) {
		gsub(/package=/,"",tags)
		gsub(/,/, "-", tags)
		gsub(/runtime-/, "", tags)
		pkg=tags
	} else {
		pkg=tags
	}
	output=pkg".plist"

	print "@("uname","gname","mode","flags") " $1 > output
}
