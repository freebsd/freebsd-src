# $FreeBSD: src/usr.sbin/jexec/Makefile,v 1.5.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	jexec
MAN=	jexec.8
DPADD=	${LIBJAIL} ${LIBUTIL}
LDADD=	-ljail -lutil
WARNS?=	6

.include <bsd.prog.mk>
