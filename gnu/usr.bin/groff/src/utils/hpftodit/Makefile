# $FreeBSD: src/gnu/usr.bin/groff/src/utils/hpftodit/Makefile,v 1.4.12.1.8.1 2008/10/02 02:57:24 kensmith Exp $

PROG_CXX=	hpftodit
SRCS=		hpftodit.cpp hpuni.cpp
DPADD=		${LIBGROFF} ${LIBM}
LDADD=		${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
