# $FreeBSD: src/usr.sbin/jexec/Makefile,v 1.5.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

PROG=	jexec
MAN=	jexec.8
DPADD=	${LIBJAIL} ${LIBUTIL}
LDADD=	-ljail -lutil
WARNS?=	6

.include <bsd.prog.mk>
