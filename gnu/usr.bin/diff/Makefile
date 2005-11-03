# $FreeBSD: src/gnu/usr.bin/diff/Makefile,v 1.16 2004/02/17 05:38:45 ache Exp $

.PATH: ${.CURDIR}/../../../contrib/diff

PROG=	diff
SRCS=   analyze.c cmpbuf.c context.c diff.c dir.c ed.c \
	ifdef.c io.c normal.c prepend_args.c side.c util.c version.c

# Important for ctype macros!
CFLAGS+=-funsigned-char

CFLAGS+=-I${.CURDIR}/../../../contrib/diff \
	-I${DESTDIR}/usr/include/gnu
CFLAGS+=-DHAVE_CONFIG_H
CFLAGS+=-DPR_PROGRAM=\"/usr/bin/pr\"

SUBDIR+=doc

DPADD=	${LIBGNUREGEX}
LDADD=	-lgnuregex

.include <bsd.prog.mk>
