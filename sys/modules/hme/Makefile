# $FreeBSD: src/sys/modules/hme/Makefile,v 1.4.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/hme

KMOD=	if_hme
SRCS=	bus_if.h device_if.h if_hme.c if_hme_pci.c ${if_hme_sbus} miibus_if.h
SRCS+=	${ofw_bus_if} pci_if.h

.if ${MACHINE} == "sparc64"
if_hme_sbus=	if_hme_sbus.c
ofw_bus_if=	ofw_bus_if.h
.endif

.include <bsd.kmod.mk>
