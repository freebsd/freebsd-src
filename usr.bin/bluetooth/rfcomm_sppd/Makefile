# $Id: Makefile,v 1.7 2003/09/07 18:15:55 max Exp $
# $FreeBSD$

PROG=		rfcomm_sppd
MAN=		rfcomm_sppd.1
SRCS=		rfcomm_sppd.c rfcomm_sdp.c
WARNS?=		2

DPADD=		${LIBBLUETOOTH} ${LIBSDP}
LDADD=		-lbluetooth -lsdp

.include <bsd.prog.mk>
