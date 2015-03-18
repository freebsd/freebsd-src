#!/usr/bin/awk
/^[^#]/ {
	gsub(/^\./,"", $1)
	uname = gname = mode = flags = tags = type = ""
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
		} else if ($i ~ /^type=dir/) {
			type="dir"
		}
	}
	if (length(tags) == 0)
		next
	if (tags ~ /package=/) {
		ext = pkgname = pkgend = ""
		split(tags, a, ",");
		for (i in a) {
			if (a[i] ~ /^package=/) {
				pkgname=a[i]
				gsub(/package=/, "", pkgname)
			} else if (a[i] == "config") {
				type="config"
			} else if (a[i] == "development" || a[i] == "profile") {
				pkgend=a[i]
			} else {
				if (ext != "")
					ext=ext"-"a[i]
				else
					ext=a[i]
			}
		}
		if (ext != "") {
			if (pkgname == "runtime") {
				pkgname=ext
			} else {
				pkgname=pkgname"-"ext
			}
		}
		if (pkgend != "") {
			if (pkgname == "runtime") {
				pkgname=pkgend
			} else {
				pkgname=pkgname"-"pkgend
			}
		}
	} else {
		print "No packages specified in line: $0" > 2
		next
	}
	output=pkgname".plist"

	print "@"type"("uname","gname","mode","flags") " $1 > output
}
