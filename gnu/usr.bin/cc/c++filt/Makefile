# $FreeBSD: src/gnu/usr.bin/cc/c++filt/Makefile,v 1.15 2002/09/17 01:48:49 peter Exp $

.include "../Makefile.inc"

.PATH: ${GCCDIR}

PROG=	c++filt
SRCS=	cp-demangle.c cplus-dem+%DIKED.c
NOMAN=	1

CFLAGS+= -DMAIN -DIN_GCC -DVERSION=\"$(version)\"

DPADD+=	${LIBCC_INT}
LDADD+=	${LIBCC_INT}

cplus-dem+%DIKED.c: cplus-dem.c
	sed -e 's/^xmalloc[ 	]/_DONT_xmalloc /g' \
	    -e 's/^xrealloc[ 	]/_DONT_xrealloc /g' \
	    ${.ALLSRC} > ${.TARGET}

CLEANFILES=	cplus-dem+%DIKED.c

.include <bsd.prog.mk>
