# $FreeBSD: src/gnu/usr.bin/groff/src/devices/grops/Makefile,v 1.4.34.1.8.1 2012/03/03 06:15:13 kensmith Exp $

PROG_CXX=	grops
SRCS=		ps.cpp psrm.cpp
DPADD=		${LIBDRIVER} ${LIBGROFF} ${LIBM}
LDADD=		${LIBDRIVER} ${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
