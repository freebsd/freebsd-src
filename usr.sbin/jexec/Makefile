# $FreeBSD: src/usr.sbin/jexec/Makefile,v 1.2.2.1.6.1 2010/12/21 17:10:29 kensmith Exp $

PROG=	jexec
MAN=	jexec.8
DPADD=	${LIBUTIL}
LDADD=	-lutil
WARNS?=	6

CFLAGS+= -DSUPPORT_OLD_XPRISON

.include <bsd.prog.mk>
