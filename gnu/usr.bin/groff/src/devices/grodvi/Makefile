# $FreeBSD: src/gnu/usr.bin/groff/src/devices/grodvi/Makefile,v 1.1.2.1 2001/04/26 17:08:22 ru Exp $

PROG=		grodvi
SRCS=		dvi.cc
DPADD=		${LIBDRIVER} ${LIBGROFF} ${LIBM}
LDADD=		${LIBDRIVER} ${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
