#	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD: src/sbin/init/Makefile,v 1.20.2.4 2001/08/01 06:37:01 obrien Exp $

PROG=	init
MAN=	init.8
MLINKS=	init.8 securelevel.8
BINMODE=500
INSTALLFLAGS=-fschg -b -B.bak
CFLAGS+=-DDEBUGSHELL -DSECURE -DLOGIN_CAP -DCOMPAT_SYSV_INIT
DPADD=	${LIBUTIL} ${LIBCRYPT}
LDADD=	-lutil -lcrypt

.include <bsd.prog.mk>
