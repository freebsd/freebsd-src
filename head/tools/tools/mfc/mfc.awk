#!/usr/bin/awk -f
#
# $FreeBSD$
#

BEGIN {
	CVSROOT="ncvs:/home/ncvs"
	UPDATEOPTS="-kk"
}

/^>/ {
	sub(">[ 	]*", "")
}

/^Revision/ || $1 == "" {
	next
}

{
	if (sub("1.", "") != 1)
		next
	if (!(match($2, "\\+[0-9]") && match($3, "-[0-9]")))
		next
	printf("cvs -d %s update %s -j 1.%d -j 1.%d %s\n", CVSROOT, UPDATEOPTS, $1 - 1, $1, $4)
	files[$4] = 1
}

END {
	for (i in files)
		fl = fl " " i
	printf("cvs -d %s diff -kk %s | less\n", CVSROOT, fl);
	printf("cvs -d %s commit %s\n", CVSROOT, fl);
}
