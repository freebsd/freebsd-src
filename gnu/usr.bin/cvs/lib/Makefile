LIB = cvs

CFLAGS += -I${.CURDIR} -I${.CURDIR}/../cvs -DFTIME_MISSING -DHAVE_TIMEZONE

SRCS =	argmatch.c error.c getopt.c sighandle.c strippath.c stripslash.c yesno.c \
	getdate.y fnmatch.c regex.c subr.c myndbm.c hash.c

.include <bsd.lib.mk>
