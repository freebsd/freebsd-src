# $Id: Makefile,v 1.7 2003/08/14 20:06:22 max Exp $
# $FreeBSD: src/usr.sbin/bluetooth/l2control/Makefile,v 1.3.28.1 2008/11/25 02:59:29 kensmith Exp $

PROG=		l2control
MAN=		l2control.8
SRCS=		l2cap.c l2control.c
WARNS?=		2

DPADD=		${LIBBLUETOOTH}
LDADD=		-lbluetooth

.include <bsd.prog.mk>
