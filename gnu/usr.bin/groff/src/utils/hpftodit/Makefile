# $FreeBSD: src/gnu/usr.bin/groff/src/utils/hpftodit/Makefile,v 1.5.8.1 2009/04/15 03:14:26 kensmith Exp $

PROG_CXX=	hpftodit
SRCS=		hpftodit.cpp hpuni.cpp
DPADD=		${LIBGROFF} ${LIBM}
LDADD=		${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
