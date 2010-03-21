# $FreeBSD: src/usr.sbin/jexec/Makefile,v 1.2.2.1.4.1 2010/02/10 00:26:20 kensmith Exp $

PROG=	jexec
MAN=	jexec.8
DPADD=	${LIBUTIL}
LDADD=	-lutil
WARNS?=	6

CFLAGS+= -DSUPPORT_OLD_XPRISON

.include <bsd.prog.mk>
