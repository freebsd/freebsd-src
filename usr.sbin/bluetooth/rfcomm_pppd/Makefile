# $Id: Makefile,v 1.7 2003/09/07 18:32:11 max Exp $
# $FreeBSD: src/usr.sbin/bluetooth/rfcomm_pppd/Makefile,v 1.2.26.1 2008/10/02 02:57:24 kensmith Exp $

.PATH:	${.CURDIR}/../../../usr.bin/bluetooth/rfcomm_sppd

PROG=		rfcomm_pppd
MAN=		rfcomm_pppd.8
SRCS=		rfcomm_pppd.c rfcomm_sdp.c
WARNS?=		2

DPADD=		${LIBBLUETOOTH} ${LIBSDP}
LDADD=		-lbluetooth -lsdp

.include <bsd.prog.mk>
