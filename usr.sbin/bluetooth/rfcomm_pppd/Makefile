# $Id: Makefile,v 1.2 2003/03/15 03:07:50 max Exp $
# $FreeBSD: src/usr.sbin/bluetooth/rfcomm_pppd/Makefile,v 1.1 2003/05/10 21:50:37 julian Exp $

DESTDIR=	/usr/sbin/
MANDIR=		../share/man/man
PROG=		rfcomm_pppd
MAN8=		rfcomm_pppd.8
WARNS?=		2
CFLAGS+=	-g -I${.CURDIR}/../../../sys/netgraph/bluetooth/include
SRCS=		rfcomm_pppd.c

.include <bsd.prog.mk>

