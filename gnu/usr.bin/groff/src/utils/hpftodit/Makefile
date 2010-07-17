# $FreeBSD: src/gnu/usr.bin/groff/src/utils/hpftodit/Makefile,v 1.5.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG_CXX=	hpftodit
SRCS=		hpftodit.cpp hpuni.cpp
DPADD=		${LIBGROFF} ${LIBM}
LDADD=		${LIBGROFF} -lm
CLEANFILES=	${MAN}

.include <bsd.prog.mk>
