# $FreeBSD: src/sys/modules/sn/Makefile,v 1.4.24.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/sn

KMOD=	if_sn
SRCS=	if_sn.c if_sn_isa.c if_sn_pccard.c device_if.h bus_if.h isa_if.h \
	card_if.h pccarddevs.h

.include <bsd.kmod.mk>
