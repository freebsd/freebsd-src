# $FreeBSD$

SHLIBDIR?=	/usr/lib

.include <bsd.own.mk>

RDMACMDIR= 	${.CURDIR}/../../librdmacm
RDMASRCDIR=	${RDMACMDIR}/src
RDMAMANDIR=	${RDMACMDIR}/man

.PATH: ${RDMASRCDIR} ${RDMAMANDIR}

LIB=		rdmacm
SHLIB_MAJOR=	1
MK_PROFILE=	no

SRCS=	cma.c

MAN=	rdma_get_devices.3 mckey.1 rdma_get_dst_port.3 rdma_accept.3
MAN+=	rdma_get_local_addr.3 rdma_ack_cm_event.3 rdma_get_peer_addr.3
MAN+=	rdma_bind_addr.3 rdma_get_src_port.3 rdma_cm.7 rdma_join_multicast.3
MAN+=	rdma_connect.3 rdma_leave_multicast.3 rdma_create_event_channel.3
MAN+=	rdma_listen.3 rdma_create_id.3 rdma_migrate_id.3 rdma_create_qp.3
MAN+=	rdma_notify.3 rdma_destroy_event_channel.3 rdma_reject.3
MAN+=	rdma_destroy_id.3 rdma_resolve_addr.3 rdma_destroy_qp.3
MAN+=	rdma_resolve_route.3 rdma_disconnect.3 rdma_set_option.3
MAN+=	rdma_event_str.3 rping.1 rdma_free_devices.3 ucmatose.1
MAN+=	rdma_get_cm_event.3 udaddy.1

LIBADD=	ibverbs

VERSION_MAP= ${RDMASRCDIR}/librdmacm.map

.include <bsd.lib.mk>
