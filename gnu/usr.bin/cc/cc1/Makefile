# $FreeBSD: src/gnu/usr.bin/cc/cc1/Makefile,v 1.18.2.2 2002/06/20 23:13:30 obrien Exp $

.include "../Makefile.inc"

.PATH: ${GCCDIR}
 
PROG=	cc1
SRCS=	c-parse.y c-lang.c
# Ugh, compiled twice...
SRCS+=	c-decl.c c-lex.c
BINDIR=	/usr/libexec
NOMAN=	1
NOSHARED=yes

CFLAGS+= -I.

DPADD+=	${LIBCC_INT} 
LDADD+=	${LIBCC_INT}

#-----------------------------------------------------------------------
# C parser
c-parse.y: c-parse.in
	sed -e "/^ifobjc$$/,/^end ifobjc$$/d" \
	    -e "/^ifc$$/d" \
	    -e "/^end ifc$$/d" \
	    ${.ALLSRC} > ${.TARGET}

CLEANFILES+=	c-parse.y
#-----------------------------------------------------------------------

.include <bsd.prog.mk>
