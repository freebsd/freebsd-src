# $FreeBSD: src/gnu/usr.bin/groff/src/devices/grops/Makefile,v 1.4.36.1 2010/02/10 00:26:20 kensmith Exp $

PROG_CXX=	grops
SRCS=		ps.cpp psrm.cpp
DPADD=		${LIBDRIVER} ${LIBGROFF} ${LIBM}
LDADD=		${LIBDRIVER} ${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
