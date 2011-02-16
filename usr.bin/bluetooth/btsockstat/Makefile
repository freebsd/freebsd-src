# $Id: Makefile,v 1.7 2003/08/14 20:07:14 max Exp $
# $FreeBSD: src/usr.bin/bluetooth/btsockstat/Makefile,v 1.4.26.1 2010/12/21 17:10:29 kensmith Exp $

PROG=		btsockstat
WARNS?=		2
BINGRP=		kmem
BINMODE=	2555

DPADD=		${LIBBLUETOOTH} ${LIBKVM}
LDADD=		-lbluetooth -lkvm

.include <bsd.prog.mk>
