# $FreeBSD$

PROG=		indxbib
SRCS=		indxbib.cc dirnamemax.c signal.c
CFLAGS+=	-I${.CURDIR}/../include
LDADD+= 	${LIBBIB} ${LIBGROFF} -lm
DPADD+= 	${LIBBIB} ${LIBGROFF} ${LIBM}

CLEANFILES+=	indxbib.1

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${NOBINMODE} \
	    ${DIST_DIR}/eign ${DESTDIR}${SHAREDIR}/dict/

.include "../Makefile.cfg"
.include <bsd.prog.mk>
