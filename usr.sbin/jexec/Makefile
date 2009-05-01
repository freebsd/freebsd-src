# $FreeBSD: src/usr.sbin/jexec/Makefile,v 1.2.2.1.2.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	jexec
MAN=	jexec.8
DPADD=	${LIBUTIL}
LDADD=	-lutil
WARNS?=	6

CFLAGS+= -DSUPPORT_OLD_XPRISON

.include <bsd.prog.mk>
