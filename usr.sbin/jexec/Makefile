# $FreeBSD: src/usr.sbin/jexec/Makefile,v 1.5.2.1.8.1 2012/03/03 06:15:13 kensmith Exp $

PROG=	jexec
MAN=	jexec.8
DPADD=	${LIBJAIL} ${LIBUTIL}
LDADD=	-ljail -lutil
WARNS?=	6

.include <bsd.prog.mk>
