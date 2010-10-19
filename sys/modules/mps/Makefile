# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/mps

KMOD=	mps
SRCS=	mps_pci.c mps.c mps_sas.c mps_table.c mps_user.c
SRCS+=	opt_mps.h opt_cam.h opt_compat.h
SRCS+=	device_if.h bus_if.h pci_if.h

#CFLAGS += -DMPS_DEBUG
DEBUG += -g

.include <bsd.kmod.mk>
