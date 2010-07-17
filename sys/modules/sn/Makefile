# $FreeBSD: src/sys/modules/sn/Makefile,v 1.4.30.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/sn

KMOD=	if_sn
SRCS=	if_sn.c if_sn_isa.c if_sn_pccard.c device_if.h bus_if.h isa_if.h \
	card_if.h pccarddevs.h

.include <bsd.kmod.mk>
