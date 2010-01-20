# From:	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD$

.include <bsd.own.mk>

.PATH: ${.CURDIR}/../../../../usr.bin/login

PROG=	login
SRCS=	pico-login.c login_fbtab.c
MAN=	login.1

CFLAGS+=-DLOGALL

DPADD=	${LIBUTIL} ${LIBCRYPT}
LDADD=	-lutil -lcrypt

.if ${MK_PAM_SUPPORT} != "no"
CFLAGS+= -DUSE_PAM
DPADD+=	${LIBPAM}
LDADD+=	${MINUSLPAM}
.endif

BINOWN=	root
BINMODE=4555
PRECIOUSPROG=

.include <bsd.prog.mk>
