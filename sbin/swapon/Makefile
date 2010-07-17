#	@(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD: src/sbin/swapon/Makefile,v 1.9.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	swapon
MAN=	swapon.8
LINKS=	${BINDIR}/swapon ${BINDIR}/swapoff
LINKS+=	${BINDIR}/swapon ${BINDIR}/swapctl
MLINKS=	swapon.8 swapoff.8
MLINKS+=swapon.8 swapctl.8

DPADD=	${LIBUTIL}
LDADD=	-lutil

.include <bsd.prog.mk>
