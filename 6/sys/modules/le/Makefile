# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/le

KMOD=	if_le
SRCS=	am7990.c am79900.c ${if_le_ledma} if_le_pci.c lance.c
SRCS+=	bus_if.h device_if.h ${ofw_bus_if} pci_if.h

.if ${MACHINE_ARCH} == "sparc64"
if_le_ledma=	if_le_ledma.c
ofw_bus_if=	ofw_bus_if.h
.endif

.include <bsd.kmod.mk>
