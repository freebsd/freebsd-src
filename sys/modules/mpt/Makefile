# $FreeBSD: src/sys/modules/mpt/Makefile,v 1.1 2002/10/31 19:39:23 jhb Exp $

.PATH: ${.CURDIR}/../../dev/mpt

KMOD=	mpt
SRCS=	bus_if.h device_if.h pci_if.h \
	opt_cam.h opt_ddb.h \
	mpt.c mpt_debug.c mpt_freebsd.c mpt_pci.c

.include <bsd.kmod.mk>
