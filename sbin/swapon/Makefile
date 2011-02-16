#	@(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD: src/sbin/swapon/Makefile,v 1.9.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

PROG=	swapon
MAN=	swapon.8
LINKS=	${BINDIR}/swapon ${BINDIR}/swapoff
LINKS+=	${BINDIR}/swapon ${BINDIR}/swapctl
MLINKS=	swapon.8 swapoff.8
MLINKS+=swapon.8 swapctl.8

DPADD=	${LIBUTIL}
LDADD=	-lutil

.include <bsd.prog.mk>
