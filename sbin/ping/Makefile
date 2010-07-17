#	@(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD: src/sbin/ping/Makefile,v 1.20.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	ping
MAN=	ping.8
BINOWN=	root
BINMODE=4555
WARNS?=	2
DPADD=	${LIBM}
LDADD=	-lm

.if !defined(RELEASE_CRUNCH)
CFLAGS+=-DIPSEC
DPADD+=	${LIBIPSEC}
LDADD+=	-lipsec
.endif

.include <bsd.prog.mk>
