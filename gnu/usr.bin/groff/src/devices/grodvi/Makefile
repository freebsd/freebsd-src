# $FreeBSD: src/gnu/usr.bin/groff/src/devices/grodvi/Makefile,v 1.3 2002/04/11 11:06:00 ru Exp $

PROG_CXX=	grodvi
SRCS=		dvi.cc
DPADD=		${LIBDRIVER} ${LIBGROFF} ${LIBM}
LDADD=		${LIBDRIVER} ${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
