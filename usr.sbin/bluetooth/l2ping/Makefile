# $Id: Makefile,v 1.2 2003/03/15 03:07:49 max Exp $
# $FreeBSD: src/usr.sbin/bluetooth/l2ping/Makefile,v 1.2 2003/05/10 21:50:36 julian Exp $

DESTDIR=	/usr/sbin/
MANDIR=		../share/man/man
PROG=		l2ping
MAN8=		l2ping.8
SRCS=		l2ping.c
WARNS?=		2
CFLAGS+=	-g -I${.CURDIR}/../../../sys/netgraph/bluetooth/include

.include <bsd.prog.mk>
