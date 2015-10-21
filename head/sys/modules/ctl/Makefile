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
SRCS+=	ctl_frontend_ioctl.c
SRCS+=	ctl_frontend_iscsi.c
SRCS+=	ctl_ha.c
SRCS+=	ctl_scsi_all.c
SRCS+=	ctl_tpc.c
SRCS+=	ctl_tpc_local.c
SRCS+=	ctl_error.c
SRCS+=	ctl_util.c
SRCS+=	scsi_ctl.c
SRCS+=	bus_if.h
SRCS+=	device_if.h
SRCS+=	vnode_if.h
SRCS+=	icl_conn_if.h
SRCS+=	opt_cam.h

#CFLAGS+=-DICL_KERNEL_PROXY

MFILES=	kern/bus_if.m kern/device_if.m dev/iscsi/icl_conn_if.m

.include <bsd.kmod.mk>
