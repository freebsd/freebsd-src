# 	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD: src/usr.bin/login/Makefile,v 1.48.2.1 2006/03/09 22:18:07 yar Exp $

PROG=	login
SRCS=	login.c login_fbtab.c
CFLAGS+=-DLOGALL
DPADD=	${LIBUTIL} ${LIBPAM}
LDADD=	-lutil ${MINUSLPAM}

.if !defined(NO_SETUID_LOGIN)
BINOWN=	root
BINMODE=4555
PRECIOUSPROG=
.endif

.include <bsd.prog.mk>
