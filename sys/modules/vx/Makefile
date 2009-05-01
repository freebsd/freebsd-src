# $FreeBSD: src/sys/modules/vx/Makefile,v 1.4.8.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/vx

KMOD=	if_vx
SRCS=	if_vx.c if_vx_pci.c
.if ${MACHINE_ARCH} == "i386"
SRCS+=	if_vx_eisa.c
.endif
SRCS+=	device_if.h bus_if.h pci_if.h eisa_if.h

.include <bsd.kmod.mk>
