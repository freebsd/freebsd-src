# $FreeBSD: src/gnu/usr.bin/groff/indxbib/Makefile,v 1.8.6.1 2000/12/07 09:57:09 ru Exp $

PROG=		indxbib
SRCS=		indxbib.cc dirnamemax.c signal.c
CFLAGS+=	-I${.CURDIR}/../include
LDADD+= 	${LIBBIB} ${LIBGROFF} -lm
DPADD+= 	${LIBBIB} ${LIBGROFF} ${LIBM}

CLEANFILES+=	${MAN1}

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${NOBINMODE} \
	    ${DIST_DIR}/eign ${DESTDIR}${SHAREDIR}/dict/

.include "../Makefile.cfg"
.include <bsd.prog.mk>
