# $FreeBSD$

.include "../Makefile.inc"

.PATH: ../cc_tools ${GCCDIR}
 
PROG=	cc1
SRCS=	main.c c-parse.c c-lang.c c-decl.c
BINDIR=	/usr/libexec
NOMAN=	1
NOSHARED?=yes

CFLAGS+= -I.

DPADD+=	${LIBCC_INT} 
LDADD+=	${LIBCC_INT}

#-----------------------------------------------------------------------
# C parser
.ORDER: c-parse.c
c-parse.c: c-parse.in
	sed -e "/^ifobjc$$/,/^end ifobjc$$/d" \
	    -e "/^ifc$$/d" \
	    -e "/^end ifc$$/d" \
	    ${GCCDIR}/c-parse.in > c-parse.y
	${YACC} -o c-parse.c.in c-parse.y
	sed -e "s/malloc/xmalloc/g" \
	    -e "s/realloc/xrealloc/g" \
	    c-parse.c.in >c-parse.c

CLEANFILES+=	c-parse.c           c-parse.y		# insurance

#-----------------------------------------------------------------------

beforeinstall:
	mkdir -p ${DESTDIR}/usr/include/machine
.for VAHDR in stdarg.h varargs.h
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${GCCDIR}/ginclude/${VAHDR} \
	    ${DESTDIR}/usr/include/machine/gcc_${VAHDR}
.endfor

.include <bsd.prog.mk>
