#
# $FreeBSD: src/gnu/usr.bin/cc/cc1obj/Makefile,v 1.5.2.1 1999/08/29 14:32:34 peter Exp $
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
