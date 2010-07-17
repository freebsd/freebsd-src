# $FreeBSD: src/sys/modules/mpt/Makefile,v 1.3.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/mpt

KMOD=	mpt
SRCS=	bus_if.h device_if.h pci_if.h \
	opt_cam.h opt_ddb.h \
	mpt.c mpt_cam.c mpt_debug.c mpt_pci.c mpt_raid.c mpt_user.c

.include <bsd.kmod.mk>
