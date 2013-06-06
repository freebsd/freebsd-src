# $FreeBSD$

DIFFSRC=${.CURDIR}/../../../contrib/diff/src
.PATH: ${DIFFSRC} \
       ${.CURDIR}/../../../contrib/diff/lib \
       ${.CURDIR}/../../../contrib/diff/man

PROG=	diff
SRCS=   analyze.c context.c diff.c dir.c ed.c ifdef.c io.c \
	normal.c side.c util.c \
	xmalloc.c strtoumax.c cmpbuf.c exitfail.c error.c quotesys.c \
	strftime.c c-stack.c basename.c exclude.c hard-locale.c \
	file-type.c posixver.c prepargs.c version-etc.c

# Important for ctype macros!
CFLAGS+=-funsigned-char

CFLAGS+=-DHAVE_CONFIG_H
CFLAGS+=-DPR_PROGRAM=\"/usr/bin/pr\"
CFLAGS+=-I${.CURDIR}/../../../contrib/diff
CFLAGS+=-I${.CURDIR}/../../../contrib/diff/src
CFLAGS+=-I${.CURDIR}/../../../contrib/diff/lib
CFLAGS+=-I${DESTDIR}/usr/include/gnu

SUBDIR+=doc

DPADD=	${LIBGNUREGEX}
LDADD=	-lgnuregex

.include <bsd.prog.mk>
