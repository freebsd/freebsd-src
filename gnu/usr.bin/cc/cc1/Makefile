#
# $Id: Makefile,v 1.9.2.1 1997/06/29 06:05:03 pst Exp $
#
 
PROG =	cc1
SRCS =	c-parse.c \
	c-aux-info.c c-convert.c c-decl.c c-iterate.c c-lang.c c-lex.c \
	c-typeck.c
BINDIR=	/usr/libexec
NOMAN=	1
NOSHARED=yes
DPADD+=	${LIBCC_INT} 
LDADD+=	${LIBCC_INT}

.include <bsd.prog.mk>
