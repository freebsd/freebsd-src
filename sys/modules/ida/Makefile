# $FreeBSD: src/sys/modules/ida/Makefile,v 1.1.2.1 2003/09/01 06:59:02 scottl Exp $

.PATH: ${.CURDIR}/../../dev/ida

KMOD=	ida
SRCS=	ida.c ida_disk.c ida_pci.c

.if !defined(IDA_NO_EISA)
SRCS+=  ida_eisa.c
.endif

SRCS+=	device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
