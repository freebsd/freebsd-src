# $FreeBSD: src/lib/libproc/test/t3-name2sym/Makefile,v 1.1.2.2.4.1 2012/03/03 06:15:13 kensmith Exp $

PROG=	t3-name2sym

SRCS=	t3-name2sym.c

LDADD=	-lproc -lelf -lrtld_db -lutil
DPADD=	${LIBPROC} ${LIBELF}

WITHOUT_MAN=

.include <bsd.prog.mk>
