# $FreeBSD: src/gnu/usr.bin/groff/src/devices/grops/Makefile,v 1.1.2.1 2001/04/26 17:08:26 ru Exp $

PROG=		grops
SRCS=		ps.cc psrm.cc
DPADD=		${LIBDRIVER} ${LIBGROFF} ${LIBM}
LDADD=		${LIBDRIVER} ${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
