# $FreeBSD$

.PATH: ${.CURDIR}/../../../contrib/diff

PROG=	diff
SRCS=	analyze.c cmpbuf.c context.c diff.c dir.c ed.c getopt.c getopt1.c \
	ifdef.c io.c normal.c prepend_args.c side.c util.c version.c

# Important for ctype macros!
CFLAGS+=-funsigned-char

CFLAGS+=-I${.CURDIR}/../../../contrib/diff
CFLAGS+=-DHAVE_CONFIG_H
CFLAGS+=-DPR_PROGRAM=\"/usr/bin/pr\"

SUBDIR+=doc

DPADD+=	${LIBGNUREGEX}
LDADD+=	-lgnuregex

.include <bsd.prog.mk>
