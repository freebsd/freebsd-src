# $FreeBSD: src/gnu/usr.bin/groff/src/utils/indxbib/Makefile,v 1.6 2002/07/29 09:40:08 ru Exp $

PROG_CXX=	indxbib
SRCS=		indxbib.cc signal.c
DPADD=		${LIBBIB} ${LIBGROFF} ${LIBM}
LDADD=		${LIBBIB} ${LIBGROFF} -lm
CLEANFILES=	${MAN}

beforeinstall:
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${NOBINMODE} \
	    ${DIST_DIR}/eign ${DESTDIR}${SHAREDIR}/dict/

.include <bsd.prog.mk>
