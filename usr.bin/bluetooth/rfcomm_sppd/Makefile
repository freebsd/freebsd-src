# $Id: Makefile,v 1.2 2003/04/26 23:55:34 max Exp $
# $FreeBSD: src/usr.bin/bluetooth/rfcomm_sppd/Makefile,v 1.1 2003/05/10 21:44:42 julian Exp $

PROG=		rfcomm_sppd
MAN1=		rfcomm_sppd.1

DESTDIR=        /usr/bin/
MANDIR=         ../share/man/man

WARNS?=		2
CFLAGS+=	-g -I${.CURDIR}/../../../sys/netgraph/bluetooth/include/

SRCS=		rfcomm_sppd.c

.include <bsd.prog.mk>
