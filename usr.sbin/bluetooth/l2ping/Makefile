# $Id: Makefile,v 1.6 2003/08/14 20:06:24 max Exp $
# $FreeBSD$

PROG=		l2ping
MAN=		l2ping.8
SRCS=		l2ping.c
WARNS?=		2

DPADD=		${LIBBLUETOOTH}
LDADD=		-lbluetooth

.include <bsd.prog.mk>
