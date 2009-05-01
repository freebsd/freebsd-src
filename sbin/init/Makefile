#	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD: src/sbin/init/Makefile,v 1.34.8.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	init
MAN=	init.8
PRECIOUSPROG=
INSTALLFLAGS=-b -B.bak
WARNS?=	6
CFLAGS+=-DDEBUGSHELL -DSECURE -DLOGIN_CAP -DCOMPAT_SYSV_INIT
DPADD=	${LIBUTIL} ${LIBCRYPT}
LDADD=	-lutil -lcrypt

NO_SHARED?=	YES

.include <bsd.prog.mk>
