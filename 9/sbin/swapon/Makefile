#	@(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD$

PROG=	swapon
MAN=	swapon.8
LINKS=	${BINDIR}/swapon ${BINDIR}/swapoff
LINKS+=	${BINDIR}/swapon ${BINDIR}/swapctl
MLINKS=	swapon.8 swapoff.8
MLINKS+=swapon.8 swapctl.8

WARNS?=	3
DPADD=	${LIBUTIL}
LDADD=	-lutil

.include <bsd.prog.mk>
