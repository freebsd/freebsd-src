# $FreeBSD$

PROG=		indxbib
SRCS=		indxbib.cc signal.c
DPADD=		${LIBBIB} ${LIBGROFF} ${LIBM} ${LIBSTDCPLUSPLUS}
LDADD=		${LIBBIB} ${LIBGROFF} -lm -lstdc++
CLEANFILES=	${MAN}

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${NOBINMODE} \
	    ${DIST_DIR}/eign ${DESTDIR}${SHAREDIR}/dict/

.include <bsd.prog.mk>
