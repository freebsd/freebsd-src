# $FreeBSD: src/gnu/usr.bin/groff/src/devices/grotty/Makefile,v 1.1.2.3 2003/06/26 13:13:54 ru Exp $

PROG_CXX=	grotty
SRCS=		tty.cpp
DPADD=		${LIBDRIVER} ${LIBGROFF} ${LIBM}
LDADD=		${LIBDRIVER} ${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
