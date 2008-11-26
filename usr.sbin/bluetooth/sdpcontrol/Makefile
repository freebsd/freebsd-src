# $Id: Makefile,v 1.1 2003/09/08 02:27:27 max Exp $
# $FreeBSD: src/usr.sbin/bluetooth/sdpcontrol/Makefile,v 1.1.26.1 2008/10/02 02:57:24 kensmith Exp $

PROG=		sdpcontrol
MAN=		sdpcontrol.8
SRCS=		sdpcontrol.c search.c
WARNS?=		2

DPADD=		${LIBBLUETOOTH} ${LIBSDP}
LDADD=		-lbluetooth -lsdp

.include <bsd.prog.mk>
