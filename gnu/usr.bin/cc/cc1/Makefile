#
# $Id: Makefile,v 1.11 1997/02/22 15:44:53 peter Exp $
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
