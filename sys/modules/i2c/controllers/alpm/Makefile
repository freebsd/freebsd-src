# $FreeBSD: src/sys/modules/i2c/controllers/alpm/Makefile,v 1.1 2002/03/23 15:48:39 nsouch Exp $

.PATH:		${.CURDIR}/../../../../pci
KMOD		= alpm
CFLAGS		+= -DALPM_SMBIO_BASE_ADDR=0xbc00
SRCS		= device_if.h bus_if.h iicbus_if.h smbus_if.h pci_if.h \
		  alpm.c

.include <bsd.kmod.mk>
