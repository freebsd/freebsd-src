#	$Id: Makefile,v 1.25 1995/03/22 19:39:45 nate Exp $

LIB = cvs
NOPROFILE= yes
CFLAGS += -I${.CURDIR} -I${.CURDIR}/../cvs -DHAVE_CONFIG_H -DHAVE_TIMEZONE
SRCS =	argmatch.c error.c getdate.y getopt.c getopt1.c fnmatch.c myndbm.c \
	hash.c sighandle.c strippath.c stripslash.c subr.c version.c yesno.c

CLEANFILES+= getdate.c y.tab.h

install:
	@echo -n

.include <bsd.lib.mk>
