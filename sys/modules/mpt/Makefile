# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/mpt

KMOD=	mpt
SRCS=	bus_if.h device_if.h pci_if.h \
	opt_cam.h opt_ddb.h \
	mpt.c mpt_debug.c mpt_freebsd.c mpt_pci.c

.include <bsd.kmod.mk>
