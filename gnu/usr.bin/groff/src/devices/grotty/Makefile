# $FreeBSD: src/gnu/usr.bin/groff/src/devices/grotty/Makefile,v 1.1.2.1 2001/04/26 17:08:26 ru Exp $

PROG=		grotty
SRCS=		tty.cc
DPADD=		${LIBDRIVER} ${LIBGROFF} ${LIBM}
LDADD=		${LIBDRIVER} ${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
