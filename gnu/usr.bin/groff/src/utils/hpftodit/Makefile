# $FreeBSD: src/gnu/usr.bin/groff/src/utils/hpftodit/Makefile,v 1.5.6.1 2008/11/25 02:59:29 kensmith Exp $

PROG_CXX=	hpftodit
SRCS=		hpftodit.cpp hpuni.cpp
DPADD=		${LIBGROFF} ${LIBM}
LDADD=		${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
