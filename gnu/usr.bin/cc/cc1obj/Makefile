#
# $Id: Makefile,v 1.2 1996/09/21 14:27:29 peter Exp $
#

PROG =	cc1obj
SRCS =	objc-parse.c objc-act.c
# Ugh, compiled twice...
SRCS +=	c-aux-info.c c-convert.c c-decl.c c-iterate.c c-lex.c c-typeck.c
BINDIR=	/usr/libexec
NOMAN=	1
NOSHARED=yes
DPADD+=	${LIBCC_INT} 
LDADD+=	${LIBCC_INT}

.include <bsd.prog.mk>
