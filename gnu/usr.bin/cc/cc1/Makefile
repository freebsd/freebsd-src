#	$Id: Makefile,v 1.13 1999/04/04 16:36:32 obrien Exp $

.include "../Makefile.inc"

.PATH: ../cc_tools ${GCCDIR}
 
PROG=	cc1
SRCS=	c-parse.c \
	c-aux-info.c c-common.c c-convert.c c-decl.c c-iterate.c c-lang.c \
	c-lex.c c-pragma.c c-typeck.c
BINDIR=	/usr/libexec
NOMAN=	1
NOSHARED=yes
DPADD+=	${LIBCC_INT} 
LDADD+=	${LIBCC_INT}

.include <bsd.prog.mk>
