# $FreeBSD: src/gnu/usr.bin/groff/src/devices/grotty/Makefile,v 1.3 2002/04/11 11:06:03 ru Exp $

PROG_CXX=	grotty
SRCS=		tty.cc
DPADD=		${LIBDRIVER} ${LIBGROFF} ${LIBM}
LDADD=		${LIBDRIVER} ${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
