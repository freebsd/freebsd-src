# $Id: Makefile,v 1.7 2003/08/14 20:07:14 max Exp $
# $FreeBSD: src/usr.bin/bluetooth/btsockstat/Makefile,v 1.4.18.1 2008/11/25 02:59:29 kensmith Exp $

PROG=		btsockstat
WARNS?=		2
BINGRP=		kmem
BINMODE=	2555

DPADD=		${LIBBLUETOOTH} ${LIBKVM}
LDADD=		-lbluetooth -lkvm

.include <bsd.prog.mk>
