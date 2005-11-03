# $FreeBSD: src/sys/modules/mpt/Makefile,v 1.2 2005/07/10 15:07:57 scottl Exp $

.PATH: ${.CURDIR}/../../dev/mpt

KMOD=	mpt
SRCS=	bus_if.h device_if.h pci_if.h \
	opt_cam.h opt_ddb.h \
	mpt.c mpt_cam.c mpt_debug.c mpt_pci.c mpt_raid.c

.include <bsd.kmod.mk>
