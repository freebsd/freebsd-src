#	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD: src/sbin/init/Makefile,v 1.32.2.1 2006/12/03 19:01:42 luigi Exp $

PROG=	init
MAN=	init.8
MLINKS=	init.8 securelevel.8
PRECIOUSPROG=
INSTALLFLAGS=-b -B.bak
WARNS?=	6
CFLAGS+=-DDEBUGSHELL -DSECURE -DLOGIN_CAP -DCOMPAT_SYSV_INIT
DPADD=	${LIBUTIL} ${LIBCRYPT}
LDADD=	-lutil -lcrypt

NO_SHARED?=	YES

.include <bsd.prog.mk>
