# $FreeBSD: src/gnu/usr.bin/groff/src/devices/grodvi/Makefile,v 1.1.2.3 2003/06/26 13:13:53 ru Exp $

PROG_CXX=	grodvi
SRCS=		dvi.cpp
DPADD=		${LIBDRIVER} ${LIBGROFF} ${LIBM}
LDADD=		${LIBDRIVER} ${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
