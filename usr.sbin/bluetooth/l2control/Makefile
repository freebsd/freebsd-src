# $Id: Makefile,v 1.2 2003/03/15 03:07:47 max Exp $
# $FreeBSD: src/usr.sbin/bluetooth/l2control/Makefile,v 1.2 2003/05/10 21:50:35 julian Exp $

DESTDIR=	/usr/sbin/
MANDIR=		../share/man/man
PROG=		l2control
MAN8=		l2control.8
WARNS?=		2
CFLAGS+=	-g -I${.CURDIR}/../../../sys/netgraph/bluetooth/include
SRCS=		l2cap.c l2control.c

.include <bsd.prog.mk>
