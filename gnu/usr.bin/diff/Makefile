# $FreeBSD$
PROG=	diff
SRCS=	diff.c analyze.c io.c context.c ed.c normal.c ifdef.c util.c dir.c \
	version.c getopt.c getopt1.c side.c cmpbuf.c
CFLAGS+=-DHAVE_CONFIG_H

SUBDIR+= doc

DPADD+= ${LIBGNUREGEX}
LDADD+= -lgnuregex

.include <bsd.prog.mk>
