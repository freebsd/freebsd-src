# $FreeBSD: src/gnu/usr.bin/groff/src/devices/grops/Makefile,v 1.3 2002/04/11 11:06:02 ru Exp $

PROG_CXX=	grops
SRCS=		ps.cc psrm.cc
DPADD=		${LIBDRIVER} ${LIBGROFF} ${LIBM}
LDADD=		${LIBDRIVER} ${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
