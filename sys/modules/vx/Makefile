# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/vx

KMOD=	if_vx
SRCS=	if_vx.c if_vx_pci.c
.if ${MACHINE_CPUARCH} == "i386"
SRCS+=	if_vx_eisa.c
.endif
SRCS+=	device_if.h bus_if.h pci_if.h eisa_if.h

.include <bsd.kmod.mk>
