# $Id: Makefile,v 1.3 2003/03/24 23:59:49 max Exp $
# $FreeBSD: src/usr.bin/bluetooth/btsockstat/Makefile,v 1.2 2003/05/10 21:44:42 julian Exp $

PROG=		btsockstat
BINGRP=		kmem
BINMODE=	2555
MAN1=		btsockstat.1

DESTDIR=        /usr/bin/
MANDIR=         ../share/man/man

WARNS?=		2
CFLAGS+=	-g -I${.CURDIR}/../../../sys/netgraph/bluetooth/include/

SRCS=		btsockstat.c
DPADD=		${LIBKVM}
LDADD=		-lkvm 

.include <bsd.prog.mk>
