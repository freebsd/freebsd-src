# $FreeBSD: src/gnu/usr.bin/groff/src/utils/indxbib/Makefile,v 1.1.2.2 2001/08/06 17:02:13 ru Exp $

PROG=		indxbib
SRCS=		indxbib.cc signal.c
DPADD=		${LIBBIB} ${LIBGROFF} ${LIBM}
LDADD=		${LIBBIB} ${LIBGROFF} -lm
CLEANFILES=	${MAN}

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${NOBINMODE} \
	    ${DIST_DIR}/eign ${DESTDIR}${SHAREDIR}/dict/

.include <bsd.prog.mk>
