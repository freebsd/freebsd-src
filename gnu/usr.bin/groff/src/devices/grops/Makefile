# $FreeBSD: src/gnu/usr.bin/groff/src/devices/grops/Makefile,v 1.4.28.1 2008/10/02 02:57:24 kensmith Exp $

PROG_CXX=	grops
SRCS=		ps.cpp psrm.cpp
DPADD=		${LIBDRIVER} ${LIBGROFF} ${LIBM}
LDADD=		${LIBDRIVER} ${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
