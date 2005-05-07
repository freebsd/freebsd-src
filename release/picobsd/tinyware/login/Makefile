# From:	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD: src/release/picobsd/tinyware/login/Makefile,v 1.1 2001/10/01 16:58:28 luigi Exp $

.PATH: ${.CURDIR}/../../../../usr.bin/login

PROG=	login
SRCS=	pico-login.c login_access.c login_fbtab.c
MAN=	login.1 login.access.5

CFLAGS+=-DLOGIN_ACCESS -DLOGALL

DPADD=	${LIBUTIL} ${LIBCRYPT}
LDADD=	-lutil -lcrypt

.if !defined(NOPAM)
CFLAGS+= -DUSE_PAM
DPADD+=	${LIBPAM}
LDADD+=	${MINUSLPAM}
.endif

BINOWN=	root
BINMODE=4555
INSTALLFLAGS=-fschg
NEED_LIBNAMES= yes

.include <bsd.prog.mk>
