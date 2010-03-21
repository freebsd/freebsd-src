# $FreeBSD: src/sys/modules/mpt/Makefile,v 1.2.10.1.6.1 2010/02/10 00:26:20 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/mpt

KMOD=	mpt
SRCS=	bus_if.h device_if.h pci_if.h \
	opt_cam.h opt_ddb.h \
	mpt.c mpt_cam.c mpt_debug.c mpt_pci.c mpt_raid.c mpt_user.c

.include <bsd.kmod.mk>
