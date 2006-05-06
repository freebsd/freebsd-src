# $FreeBSD: src/gnu/usr.bin/groff/src/devices/grolbp/Makefile,v 1.4 2003/05/01 13:22:18 ru Exp $

PROG_CXX=	grolbp
SRCS=		lbp.cpp
DPADD=		${LIBDRIVER} ${LIBGROFF} ${LIBM}
LDADD=		${LIBDRIVER} ${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
