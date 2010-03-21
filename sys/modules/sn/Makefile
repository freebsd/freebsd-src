# $FreeBSD: src/sys/modules/sn/Makefile,v 1.4.32.1 2010/02/10 00:26:20 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/sn

KMOD=	if_sn
SRCS=	if_sn.c if_sn_isa.c if_sn_pccard.c device_if.h bus_if.h isa_if.h \
	card_if.h pccarddevs.h

.include <bsd.kmod.mk>
