# $FreeBSD: src/sys/modules/i2c/controllers/viapm/Makefile,v 1.1.14.1 2005/11/08 17:03:55 jhb Exp $

.PATH:		${.CURDIR}/../../../../pci
KMOD		= viapm
SRCS		= device_if.h bus_if.h isa_if.h iicbb_if.h pci_if.h smbus_if.h \
		  opt_isa.h viapm.c
CFLAGS		+= -DVIAPM_BASE_ADDR=0x6000

.include <bsd.kmod.mk>
