# $Id: Makefile,v 1.1 2003/09/08 02:27:27 max Exp $
# $FreeBSD: src/usr.sbin/bluetooth/sdpcontrol/Makefile,v 1.1.36.1 2010/12/21 17:10:29 kensmith Exp $

PROG=		sdpcontrol
MAN=		sdpcontrol.8
SRCS=		sdpcontrol.c search.c
WARNS?=		2

DPADD=		${LIBBLUETOOTH} ${LIBSDP}
LDADD=		-lbluetooth -lsdp

.include <bsd.prog.mk>
