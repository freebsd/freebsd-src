# $FreeBSD: src/gnu/usr.bin/groff/src/devices/grotty/Makefile,v 1.1.2.2 2002/07/19 08:34:09 ru Exp $

PROG_CXX=	grotty
SRCS=		tty.cc
DPADD=		${LIBDRIVER} ${LIBGROFF} ${LIBM}
LDADD=		${LIBDRIVER} ${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
