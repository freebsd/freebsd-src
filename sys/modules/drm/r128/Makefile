# $FreeBSD$

.PATH:	${.CURDIR}/../../../dev/drm
KMOD	= r128
NOMAN	= YES
SRCS	= r128_cce.c r128_drv.c r128_state.c
SRCS	+=device_if.h bus_if.h pci_if.h opt_drm.h opt_drm_linux.h
KMODDEPS= agp

.include <bsd.kmod.mk>
