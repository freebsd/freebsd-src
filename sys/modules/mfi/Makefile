# $FreeBSD: src/sys/modules/mfi/Makefile,v 1.1.2.1 2006/04/04 03:24:49 scottl Exp $

.PATH: ${.CURDIR}/../../dev/mfi

KMOD=	mfi
SRCS=	mfi.c mfi_pci.c mfi_disk.c
SRCS+=	opt_mfi.h
SRCS+=	device_if.h bus_if.h pci_if.h
CFLAGS+= -Wall -Werror

# To enable debug output from the driver, uncomment these two lines.
#CFLAGS+= -DMFI_DEBUG=2
#SRCS+=	mfi_debug.c

.include <bsd.kmod.mk>
