# $FreeBSD: src/sys/modules/i2c/controllers/intpm/Makefile,v 1.1.32.1 2008/11/25 02:59:29 kensmith Exp $

.PATH:		${.CURDIR}/../../../../pci
KMOD		= intpm
SRCS		= device_if.h bus_if.h iicbus_if.h smbus_if.h pci_if.h \
		  opt_intpm.h intpmreg.h intpm.c

.include <bsd.kmod.mk>
