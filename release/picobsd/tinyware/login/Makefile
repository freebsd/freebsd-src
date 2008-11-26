# From:	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD: src/release/picobsd/tinyware/login/Makefile,v 1.4.2.1.4.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../../../usr.bin/login

PROG=	login
SRCS=	pico-login.c login_fbtab.c
MAN=	login.1 login.access.5

CFLAGS+=-DLOGALL

DPADD=	${LIBUTIL} ${LIBCRYPT}
LDADD=	-lutil -lcrypt

.if !defined(NO_PAM)
CFLAGS+= -DUSE_PAM
DPADD+=	${LIBPAM}
LDADD+=	${MINUSLPAM}
.endif

BINOWN=	root
BINMODE=4555
PRECIOUSPROG=

.include <bsd.prog.mk>
