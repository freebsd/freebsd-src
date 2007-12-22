#	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD$

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
