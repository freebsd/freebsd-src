# 	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD: src/usr.bin/login/Makefile,v 1.42 2002/04/21 12:43:14 bde Exp $

PROG=	login
SRCS=	login.c login_fbtab.c
CFLAGS+=-DLOGALL
DPADD=	${LIBUTIL} ${LIBCRYPT} ${LIBPAM}
LDADD=	-lutil -lcrypt ${MINUSLPAM}
MAN=	login.1 login.access.5
BINOWN=	root
BINMODE=4555
INSTALLFLAGS=-fschg
NEED_LIBNAMES= yes

.include <bsd.prog.mk>
