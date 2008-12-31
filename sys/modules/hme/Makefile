# $FreeBSD: src/sys/modules/hme/Makefile,v 1.3.18.1.2.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/hme

KMOD=	if_hme
SRCS=	bus_if.h device_if.h if_hme.c if_hme_pci.c ${if_hme_sbus} miibus_if.h
SRCS+=	${ofw_bus_if} pci_if.h

.if ${MACHINE} == "sparc64"
if_hme_sbus=	if_hme_sbus.c
ofw_bus_if=	ofw_bus_if.h
.endif

.include <bsd.kmod.mk>
