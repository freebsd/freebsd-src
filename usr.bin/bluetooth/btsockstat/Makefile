# $Id: Makefile,v 1.1.1.1 2002/09/09 16:12:49 max Exp $
# $FreeBSD$

PROG=		btsockstat
MAN1=		btsockstat.1

DESTDIR=        /usr/bin/
MANDIR=         ../share/man/man

WARNS?=		2
CFLAGS+=	-g -I../../../sys/netgraph/bluetooth/include/

SRCS=		btsockstat.c
DPADD=		${LIBKVM}
LDADD=		-lkvm 

.include <bsd.prog.mk>
