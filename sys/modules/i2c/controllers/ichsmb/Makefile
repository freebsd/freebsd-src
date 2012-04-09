#$FreeBSD: src/sys/modules/i2c/controllers/ichsmb/Makefile,v 1.1.24.1.8.1 2012/03/03 06:15:13 kensmith Exp $

.PATH:		${.CURDIR}/../../../../dev/ichsmb
KMOD		= ichsmb
SRCS		= device_if.h bus_if.h iicbb_if.h pci_if.h smbus_if.h \
		  ichsmb.c ichsmb_pci.c ichsmb_reg.h ichsmb_var.h

.include <bsd.kmod.mk>
