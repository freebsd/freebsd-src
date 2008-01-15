# $FreeBSD: src/sys/modules/mfi/Makefile,v 1.1.2.5 2007/06/05 22:54:01 scottl Exp $

.PATH: ${.CURDIR}/../../dev/mfi

SUBDIR= mfip

.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "amd64"
SUBDIR+= mfi_linux
.endif

KMOD=	mfi
SRCS=	mfi.c mfi_pci.c mfi_disk.c mfi_debug.c
SRCS+=	opt_mfi.h
SRCS+=	device_if.h bus_if.h pci_if.h

#CFLAGS += -DMFI_DEBUG

.include <bsd.kmod.mk>
