#	$Id: Makefile,v 1.8 1995/03/31 07:55:28 nate Exp $

LIB = cvs
NOPROFILE= yes
CFLAGS += -I${.CURDIR} -I${.CURDIR}/../cvs -DHAVE_CONFIG_H
SRCS =	argmatch.c error.c filesubr.c getdate.y getline.c \
	getopt.c getopt1.c hash.c md5.c myndbm.c run.c save-cwd.c \
	sighandle.c strippath.c stripslash.c subr.c version.c \
	xgetwd.c yesno.c

CLEANFILES+= getdate.c y.tab.h

install:
	@echo -n

.include <bsd.lib.mk>

