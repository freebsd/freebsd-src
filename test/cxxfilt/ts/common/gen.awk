#!/usr/bin/awk -f
#
# $Id$

BEGIN {
    FS = "\""
    tp = 0
    print "#!/bin/sh\n"
}

{
    sub(/#.*/, "");
    if (NF >= 5) {
	tp++
	printf("tp%d()\n{\n  run \"%s\" \"%s\"\n}\n\n", tp, $2, $4);
    }
}

END {
    print "tet_startup=\"\""
    print "tet_cleanup=\"\"\n"
    printf("%s", "iclist=\"");
    for (i = 1; i <= tp; i++) {
	printf("ic%d", i);
	if (i != tp)
	    printf(" ");
    }
    printf("\"\n\n");
    for (i = 1; i <= tp; i++)
	printf("ic%d=\"tp%d\"\n", i, i);
    print "\n. $TET_SUITE_ROOT/ts/common/func.sh";
    print ". $TET_ROOT/lib/xpg3sh/tcm.sh";
}
