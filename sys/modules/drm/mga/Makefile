# $FreeBSD$

.PATH:	${.CURDIR}/../../../dev/drm
KMOD	= mga
NOMAN	= YES
SRCS	= mga_drv.c mga_state.c mga_warp.c mga_dma.c
SRCS	+=device_if.h bus_if.h pci_if.h opt_drm.h
KMODDEPS= agp

.include <bsd.kmod.mk>
