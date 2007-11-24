# $FreeBSD$

.PATH:	${.CURDIR}/../../../dev/drm
KMOD	= drm
SRCS	= \
	ati_pcigart.c \
	drm_agpsupport.c \
	drm_auth.c \
	drm_bufs.c \
	drm_context.c \
	drm_dma.c \
	drm_drawable.c \
	drm_drv.c \
	drm_fops.c \
	drm_ioctl.c \
	drm_irq.c \
	drm_lock.c \
	drm_memory.c \
	drm_pci.c \
	drm_scatter.c \
	drm_sysctl.c \
	drm_vm.c

SRCS	+=device_if.h bus_if.h pci_if.h opt_drm.h

.include <bsd.kmod.mk>
