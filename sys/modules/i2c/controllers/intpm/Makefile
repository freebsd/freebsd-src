# $FreeBSD: src/sys/modules/i2c/controllers/intpm/Makefile,v 1.1.30.1 2008/10/02 02:57:24 kensmith Exp $

.PATH:		${.CURDIR}/../../../../pci
KMOD		= intpm
SRCS		= device_if.h bus_if.h iicbus_if.h smbus_if.h pci_if.h \
		  opt_intpm.h intpmreg.h intpm.c

.include <bsd.kmod.mk>
