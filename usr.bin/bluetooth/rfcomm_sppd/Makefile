# $Id: Makefile,v 1.7 2003/09/07 18:15:55 max Exp $
# $FreeBSD: src/usr.bin/bluetooth/rfcomm_sppd/Makefile,v 1.3.24.1 2010/02/10 00:26:20 kensmith Exp $

PROG=		rfcomm_sppd
SRCS=		rfcomm_sppd.c rfcomm_sdp.c
WARNS?=		2

DPADD=		${LIBBLUETOOTH} ${LIBSDP}
LDADD=		-lbluetooth -lsdp

.include <bsd.prog.mk>
