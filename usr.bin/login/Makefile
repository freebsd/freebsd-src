# 	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD: src/usr.bin/login/Makefile,v 1.47.2.1 2006/03/11 08:24:25 yar Exp $

PROG=	login
SRCS=	login.c login_fbtab.c
CFLAGS+=-DLOGALL
DPADD=	${LIBUTIL} ${LIBPAM}
LDADD=	-lutil ${MINUSLPAM}

.if !defined(NO_SETUID_LOGIN)
BINOWN=	root
BINMODE=4555
INSTALLFLAGS=-fschg
.endif

.include <bsd.prog.mk>
