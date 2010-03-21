# $FreeBSD: src/sys/modules/cbb/Makefile,v 1.10.32.1 2010/02/10 00:26:20 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/pccbb

KMOD=	cbb
SRCS=	pccbb.c pccbb_isa.c pccbb_pci.c \
	device_if.h bus_if.h power_if.h card_if.h isa_if.h pci_if.h pcib_if.h

.include <bsd.kmod.mk>
