# 	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD$

.include <bsd.own.mk>

PROG=	login
SRCS=	login.c login_audit.c login_fbtab.c
CFLAGS+=-DLOGALL
DPADD=	${LIBUTIL} ${LIBPAM}
LDADD=	-lutil ${MINUSLPAM} -lbsm

.if ${MK_SETUID_LOGIN} != "no"
BINOWN=	root
BINMODE=4555
PRECIOUSPROG=
.endif

.include <bsd.prog.mk>
