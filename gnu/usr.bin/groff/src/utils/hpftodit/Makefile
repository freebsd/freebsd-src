# $FreeBSD: src/gnu/usr.bin/groff/src/utils/hpftodit/Makefile,v 1.5.14.1 2010/12/21 17:10:29 kensmith Exp $

PROG_CXX=	hpftodit
SRCS=		hpftodit.cpp hpuni.cpp
DPADD=		${LIBGROFF} ${LIBM}
LDADD=		${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
