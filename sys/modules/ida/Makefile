# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/ida

KMOD=	ida
SRCS=	ida.c ida_disk.c ida_pci.c

.if !defined(IDA_NO_EISA)
SRCS+=  ida_eisa.c
.endif

SRCS+=	device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
