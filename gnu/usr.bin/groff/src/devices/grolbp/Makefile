# $FreeBSD: src/gnu/usr.bin/groff/src/devices/grolbp/Makefile,v 1.1.2.2 2002/07/19 08:34:08 ru Exp $

PROG_CXX=	grolbp
SRCS=		lbp.cc
DPADD=		${LIBDRIVER} ${LIBGROFF} ${LIBM}
LDADD=		${LIBDRIVER} ${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
