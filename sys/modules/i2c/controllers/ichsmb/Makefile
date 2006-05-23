# $FreeBSD: src/sys/modules/i2c/controllers/ichsmb/Makefile,v 1.1.4.1 2005/08/19 18:54:25 brian Exp $

.PATH:		${.CURDIR}/../../../../dev/ichsmb
KMOD		= ichsmb
SRCS		= device_if.h bus_if.h iicbb_if.h pci_if.h smbus_if.h \
		  ichsmb.c ichsmb_pci.c ichsmb_reg.h ichsmb_var.h

.include <bsd.kmod.mk>
