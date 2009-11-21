# $FreeBSD: src/gnu/usr.bin/groff/src/devices/grops/Makefile,v 1.4.34.1.2.1 2009/10/25 01:10:29 kensmith Exp $

PROG_CXX=	grops
SRCS=		ps.cpp psrm.cpp
DPADD=		${LIBDRIVER} ${LIBGROFF} ${LIBM}
LDADD=		${LIBDRIVER} ${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
