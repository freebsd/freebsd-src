# $FreeBSD: src/usr.sbin/ancontrol/Makefile,v 1.10 2002/06/06 11:30:57 ru Exp $

PROG=		ancontrol
MAN=		ancontrol.8

CFLAGS+=	-I${.CURDIR}/../../sys -DANCACHE
DPADD=		${LIBMD}
LDADD=		-lmd

WARNS?=		2

.include <bsd.prog.mk>
