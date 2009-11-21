# $FreeBSD: src/gnu/usr.bin/groff/src/devices/grotty/Makefile,v 1.4.34.1.2.1 2009/10/25 01:10:29 kensmith Exp $

PROG_CXX=	grotty
SRCS=		tty.cpp
DPADD=		${LIBDRIVER} ${LIBGROFF} ${LIBM}
LDADD=		${LIBDRIVER} ${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
