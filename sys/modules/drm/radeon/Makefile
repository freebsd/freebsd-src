# $FreeBSD$

.PATH:	${.CURDIR}/../../../dev/drm
KMOD	= radeon
NOMAN	= YES
SRCS	= radeon_cp.c radeon_drv.c radeon_state.c
SRCS	+=device_if.h bus_if.h pci_if.h opt_drm.h opt_drm_linux.h
KMODDEPS= agp

.include <bsd.kmod.mk>
