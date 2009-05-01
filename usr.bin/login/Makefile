# 	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD: src/usr.bin/login/Makefile,v 1.53.8.1 2009/04/15 03:14:26 kensmith Exp $

.include <bsd.own.mk>

PROG=	login
SRCS=	login.c login_fbtab.c
CFLAGS+=-DLOGALL
DPADD=	${LIBUTIL} ${LIBPAM}
LDADD=	-lutil ${MINUSLPAM}

.if ${MK_AUDIT} != "no"
SRCS+=	login_audit.c
CFLAGS+= -DUSE_BSM_AUDIT
DPADD+=  ${LIBBSM}
LDADD+=  -lbsm
.endif

.if ${MK_SETUID_LOGIN} != "no"
BINOWN=	root
BINMODE=4555
PRECIOUSPROG=
.endif

.include <bsd.prog.mk>
