# $FreeBSD$

.PATH:	${.CURDIR}/../../dev/iscsi/
KMOD=	iscsi

SRCS=	iscsi.c
.if defined(ICL_RDMA)
SRCS+=	icl_rdma.c
.else
SRCS+=	icl.c
.endif
SRCS+=	icl_proxy.c
SRCS+=	opt_cam.h
SRCS+=	bus_if.h
SRCS+=	device_if.h

# Those below are required for RDMA.
SRCS+=	vnode_if.h
SRCS+=	opt_inet.h
SRCS+=	opt_inet6.h

CFLAGS+= -I${.CURDIR}/../../ofed/include
#CFLAGS+=-DICL_KERNEL_PROXY

.include <bsd.kmod.mk>
