#
# $FreeBSD$
#

PROG =	cc1obj
SRCS =	objc-parse.c objc-act.c
# Ugh, compiled twice...
SRCS +=	c-aux-info.c c-convert.c c-decl.c c-iterate.c c-lex.c c-typeck.c
BINDIR=	/usr/libexec
NOMAN=	1
NOSHARED= true
DPADD+=	${LIBCC_INT} 
LDADD+=	${LIBCC_INT}

.include <bsd.prog.mk>
