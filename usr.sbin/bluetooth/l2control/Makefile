# $Id: Makefile,v 1.3 2002/09/04 21:30:40 max Exp $
# $FreeBSD: src/usr.sbin/bluetooth/l2control/Makefile,v 1.1 2002/11/20 23:01:55 julian Exp $

DESTDIR=	/usr/sbin/
MANDIR=		../share/man/man
PROG=		l2control
MAN8=		l2control.8
WARNS?=		2
CFLAGS+=	-g -I../../../sys/netgraph/bluetooth/include
SRCS=		l2cap.c l2control.c

.include <bsd.prog.mk>
