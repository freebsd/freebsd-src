#
# Makefile,v 1.5 1994/11/15 04:51:32 phk Exp
#

PROG =	cc1
SRCS =	c-aux-info.c c-convert.c c-decl.c c-iterate.c c-lang.c c-lex.c c-parse.c c-pragma.c c-typeck.c
BINDIR=	/usr/libexec
NOMAN=	1
DPADD+=	${LIBCC_INT} ${LIBGNUMALLOC}
LDADD+=	-lcc_int -lgnumalloc

.include <bsd.prog.mk>
