# $FreeBSD: src/sys/modules/i2c/controllers/intpm/Makefile,v 1.1.36.1.6.1 2010/12/21 17:09:25 kensmith Exp $

.PATH:		${.CURDIR}/../../../../pci
KMOD		= intpm
SRCS		= device_if.h bus_if.h iicbus_if.h smbus_if.h pci_if.h \
		  opt_intpm.h intpmreg.h intpm.c

.include <bsd.kmod.mk>
