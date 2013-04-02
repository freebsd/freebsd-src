# $FreeBSD$

# $FreeBSD$

.PATH: ${.CURDIR}/../../cam/ctl

KMOD=	ctl

SRCS=	ctl.c
SRCS+=	ctl_backend.c
SRCS+=	ctl_backend_block.c
SRCS+=	ctl_backend_ramdisk.c
SRCS+=	ctl_cmd_table.c
SRCS+=	ctl_frontend.c
SRCS+=	ctl_frontend_cam_sim.c
SRCS+=	ctl_frontend_internal.c
SRCS+=	ctl_mem_pool.c
SRCS+=	ctl_scsi_all.c
SRCS+=	ctl_error.c
SRCS+=	ctl_util.c
SRCS+=	scsi_ctl.c
SRCS+=	bus_if.h
SRCS+=	device_if.h
SRCS+=	vnode_if.h

.include <bsd.kmod.mk>
