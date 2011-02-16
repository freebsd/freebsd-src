# $FreeBSD: src/lib/libproc/test/t2-name2map/Makefile,v 1.1.2.2.2.1 2010/12/21 17:09:25 kensmith Exp $

PROG=	t2-name2map

SRCS=	t2-name2map.c

LDADD=	-lproc -lelf -lrtld_db -lutil
DPADD=	${LIBPROC} ${LIBELF}

WITHOUT_MAN=

.include <bsd.prog.mk>
