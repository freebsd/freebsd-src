# 	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD$

PROG=	login
SRCS=	login.c login_fbtab.c
CFLAGS+=-DLOGALL
DPADD=	${LIBUTIL} ${LIBPAM}
LDADD=	-lutil ${MINUSLPAM}
MAN=	login.1 login.access.5
.if !defined(NO_SETUID_LOGIN)
BINOWN=	root
BINMODE=4555
INSTALLFLAGS=-fschg
.endif

.include <bsd.prog.mk>
