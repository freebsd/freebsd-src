# $FreeBSD$

SHLIBDIR?=	/usr/lib

.include <bsd.own.mk>
.include "../Makefile.inc"

IBSRCDIR=	${IBVERBSDIR}/src
IBMANDIR=	${IBVERBSDIR}/man

.PATH: ${IBSRCDIR} ${IBMANDIR}

LIB=		ibverbs
SHLIB_MAJOR=	1
MK_PROFILE=	no

SRCS=	device.c init.c marshall.c verbs.c cmd.c enum_strs.c kern_abi.h \
	memory.c compat-1_0.c sysfs.c

MAN=	ibv_alloc_pd.3 ibv_asyncwatch.1 ibv_attach_mcast.3 ibv_create_ah.3 \
	ibv_create_ah_from_wc.3 ibv_create_comp_channel.3 ibv_create_cq.3 \
	ibv_create_qp.3 ibv_create_srq.3 ibv_devices.1 ibv_devinfo.1 \
	ibv_event_type_str.3 ibv_fork_init.3 ibv_get_async_event.3 \
	ibv_get_cq_event.3 ibv_get_device_guid.3 ibv_get_device_list.3 \
	ibv_get_device_name.3 ibv_modify_qp.3 ibv_modify_srq.3 \
	ibv_open_device.3 ibv_poll_cq.3 ibv_post_recv.3 ibv_post_send.3 \
	ibv_post_srq_recv.3 ibv_query_device.3 ibv_query_gid.3 \
	ibv_query_pkey.3 ibv_query_port.3 ibv_query_qp.3 ibv_query_srq.3 \
	ibv_rate_to_mult.3 ibv_rc_pingpong.1 ibv_reg_mr.3 ibv_req_notify_cq.3 \
	ibv_resize_cq.3 ibv_srq_pingpong.1 ibv_uc_pingpong.1 ibv_ud_pingpong.1 \
	ibv_query_xrc_rcv_qp.3 ibv_reg_xrc_rcv_qp.3 ibv_modify_xrc_rcv_qp.3 \
	verbs.7 ibv_create_xrc_rcv_qp.3 ibv_open_xrc_domain.3


CFLAGS+= -DHAVE_CONFIG_H -DIBV_CONFIG_DIR=\"/etc/ibverbs/\"

VERSION_MAP= ${IBSRCDIR}/libibverbs.map

.include <bsd.lib.mk>
