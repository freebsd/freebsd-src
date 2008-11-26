# $FreeBSD: src/sys/modules/vx/Makefile,v 1.3.20.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/vx

KMOD=	if_vx
SRCS=	if_vx.c if_vx_pci.c
.if ${MACHINE_ARCH} == "i386"
SRCS+=	if_vx_eisa.c
.endif
SRCS+=	opt_bdg.h device_if.h bus_if.h pci_if.h eisa_if.h

.include <bsd.kmod.mk>
