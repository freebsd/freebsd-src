#!/usr/bin/awk -f
#
# $FreeBSD: src/tools/tools/mfc/mfc.awk,v 1.2.6.1 2008/11/25 02:59:29 kensmith Exp $
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
