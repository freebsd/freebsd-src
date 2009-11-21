# $FreeBSD: src/usr.sbin/jexec/Makefile,v 1.5.2.1.2.1 2009/10/25 01:10:29 kensmith Exp $

PROG=	jexec
MAN=	jexec.8
DPADD=	${LIBJAIL} ${LIBUTIL}
LDADD=	-ljail -lutil
WARNS?=	6

.include <bsd.prog.mk>
