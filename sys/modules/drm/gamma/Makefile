# $FreeBSD$

.PATH:	${.CURDIR}/../../../dev/drm
KMOD	= gamma
NOMAN	= YES
SRCS    = gamma_drv.c gamma_dma.c
SRCS   += device_if.h bus_if.h pci_if.h opt_drm.h opt_drm_linux.h

.include <bsd.kmod.mk>
