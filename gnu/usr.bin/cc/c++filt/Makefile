# $FreeBSD$

.include "../Makefile.inc"

.PATH: ${GCCDIR}

PROG=	c++filt
SRCS=	cp-demangle.c cplus-dem+%DIKED.c
NOMAN=	1

CFLAGS+= -DSTANDALONE_DEMANGLER -DIN_GCC -DVERSION=\"$(version)\"

DPADD+=	${LIBCC_INT}
LDADD+=	${LIBCC_INT}

cplus-dem+%DIKED.c: cplus-dem.c
	sed -e 's/^xmalloc[ 	]/_DONT_xmalloc /g' \
	    -e 's/^xrealloc[ 	]/_DONT_xrealloc /g' \
	    ${.ALLSRC} > ${.TARGET}

CLEANFILES=	cplus-dem+%DIKED.c

.include <bsd.prog.mk>
