# $Id: Makefile,v 1.4 2003/08/14 20:07:13 max Exp $
# $FreeBSD: src/usr.bin/bluetooth/bthost/Makefile,v 1.1 2003/10/12 22:04:21 emax Exp $

PROG=		bthost
MAN=		bthost.1
SRCS=		bthost.c
WARNS?=		2

DPADD=		${LIBBLUETOOTH}
LDADD=		-lbluetooth 

.include <bsd.prog.mk>
