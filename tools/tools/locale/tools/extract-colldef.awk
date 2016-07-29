# $FreeBSD$

BEGIN {
	print "# Warning: Do not edit. This is automatically extracted"
	print "# from CLDR project data, obtained from http://cldr.unicode.org/"
	print "# -----------------------------------------------------------------------------"
}
$1 == "comment_char" { print $0 }
$1 == "escape_char" { print $0 }
$1 == "LC_COLLATE" {
	print $0
	while (getline line) {
		print line
		if (line == "END LC_COLLATE") {
			break
		}
	}
}
