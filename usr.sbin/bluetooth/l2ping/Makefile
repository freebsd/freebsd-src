# $Id: Makefile,v 1.6 2003/08/14 20:06:24 max Exp $
# $FreeBSD: src/usr.sbin/bluetooth/l2ping/Makefile,v 1.3 2003/10/12 22:04:23 emax Exp $

PROG=		l2ping
MAN=		l2ping.8
SRCS=		l2ping.c
WARNS?=		2

DPADD=		${LIBBLUETOOTH}
LDADD=		-lbluetooth

.include <bsd.prog.mk>
