#
# $Id: Makefile,v 1.7 1999/03/05 04:55:03 obrien Exp $
#

.include "../Makefile.inc"

.PATH: ../cc_tools ${GCCDIR}/objc ${GCCDIR}

PROG =	cc1obj
SRCS =	objc-parse.c objc-act.c
# Ugh, compiled twice...
SRCS +=	c-aux-info.c c-common.c c-convert.c c-decl.c c-iterate.c c-lex.c \
	c-pragma.c c-typeck.c
BINDIR=	/usr/libexec
NOMAN=	1
NOSHARED=yes
DPADD+=	${LIBCC_INT} 
LDADD+=	${LIBCC_INT}

CFLAGS+= -I${GCCDIR}/objc

.include <bsd.prog.mk>
