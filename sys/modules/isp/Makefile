# $FreeBSD: src/sys/modules/isp/Makefile,v 1.2 2004/08/12 17:41:31 marius Exp $

.PATH: ${.CURDIR}/../../dev/isp

KMOD=	isp
SRCS=	bus_if.h device_if.h pci_if.h \
	opt_cam.h opt_ddb.h opt_isp.h \
	isp.c isp_target.c isp_freebsd.c isp_pci.c

.if ${MACHINE_ARCH} == sparc64
SRCS+=	isp_sbus.c ofw_bus_if.h
.endif

.include <bsd.kmod.mk>
