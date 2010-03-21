# $FreeBSD: src/sys/modules/vx/Makefile,v 1.4.12.1 2010/02/10 00:26:20 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/vx

KMOD=	if_vx
SRCS=	if_vx.c if_vx_pci.c
.if ${MACHINE_ARCH} == "i386"
SRCS+=	if_vx_eisa.c
.endif
SRCS+=	device_if.h bus_if.h pci_if.h eisa_if.h

.include <bsd.kmod.mk>
