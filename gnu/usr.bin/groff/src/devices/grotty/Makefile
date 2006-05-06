# $FreeBSD: src/gnu/usr.bin/groff/src/devices/grotty/Makefile,v 1.4 2003/05/01 13:22:19 ru Exp $

PROG_CXX=	grotty
SRCS=		tty.cpp
DPADD=		${LIBDRIVER} ${LIBGROFF} ${LIBM}
LDADD=		${LIBDRIVER} ${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
