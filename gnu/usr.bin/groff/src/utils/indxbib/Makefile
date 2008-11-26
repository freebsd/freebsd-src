# $FreeBSD: src/gnu/usr.bin/groff/src/utils/indxbib/Makefile,v 1.7.28.1 2008/10/02 02:57:24 kensmith Exp $

PROG_CXX=	indxbib
SRCS=		indxbib.cpp signal.c
DPADD=		${LIBBIB} ${LIBGROFF} ${LIBM}
LDADD=		${LIBBIB} ${LIBGROFF} -lm
CLEANFILES=	${MAN}

beforeinstall:
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${NOBINMODE} \
	    ${DIST_DIR}/eign ${DESTDIR}${SHAREDIR}/dict/

.include <bsd.prog.mk>
