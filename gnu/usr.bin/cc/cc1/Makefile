# $FreeBSD$

.include "../Makefile.inc"

.PATH: ../cc_tools ${GCCDIR}
 
PROG=	cc1
SRCS=	c-parse.c c-parse.h c-lang.c
# Ugh, compiled twice...
SRCS+=	c-decl.c c-lex.c c-typeck.c
BINDIR=	/usr/libexec
NOMAN=	1
NOSHARED=yes

CFLAGS+= -I.

DPADD+=	${LIBCC_INT} 
LDADD+=	${LIBCC_INT}

#-----------------------------------------------------------------------
# C parser
.ORDER: c-parse.c c-parse.h
c-parse.c c-parse.h: c-parse.in
	sed -e "/^ifobjc$$/,/^end ifobjc$$/d" \
	    -e "/^ifc$$/d" -e "/^end ifc$$/d" \
	    ${GCCDIR}/c-parse.in > c-parse.y
	${YACC} -d -o c-parse.c c-parse.y
	mv c-parse.y c-parse.y.out

CLEANFILES+=	c-parse.c c-parse.h \
		c-parse.y c-parse.y.out		# insurance

#-----------------------------------------------------------------------

beforeinstall:
	mkdir -p ${DESTDIR}/usr/include/machine
.for VAHDR in stdarg.h varargs.h
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${GCCDIR}/ginclude/${VAHDR} \
	    ${DESTDIR}/usr/include/machine/gcc_${VAHDR}
.endfor

.include <bsd.prog.mk>
