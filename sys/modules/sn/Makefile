# $FreeBSD: src/sys/modules/sn/Makefile,v 1.3 2001/01/06 14:00:15 obrien Exp $

.PATH: ${.CURDIR}/../../dev/sn

KMOD=	if_sn
SRCS=	if_sn.c if_sn_isa.c if_sn_pccard.c device_if.h bus_if.h isa_if.h \
	card_if.h

.include <bsd.kmod.mk>
