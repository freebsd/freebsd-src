# $FreeBSD$
.PATH:  ${SRCTOP}/sys/ofed/drivers/infiniband/core

KMOD=	ibcore
SRCS= \
	ib_addr.c \
	ib_agent.c \
	ib_cache.c \
	ib_cm.c \
	ib_cma.c \
	ib_core_uverbs.c \
	ib_cq.c \
	ib_device.c \
	ib_fmr_pool.c \
	ib_iwcm.c \
	ib_iwpm_msg.c \
	ib_iwpm_util.c \
	ib_mad.c \
	ib_mad_rmpp.c \
	ib_multicast.c \
	ib_packer.c \
	ib_rdma_core.c \
	ib_roce_gid_mgmt.c \
	ib_sa_query.c \
	ib_smi.c \
	ib_sysfs.c \
	ib_ucm.c \
	ib_ucma.c \
	ib_ud_header.c \
	ib_umem.c \
	ib_user_mad.c \
	ib_uverbs_cmd.c \
	ib_uverbs_ioctl.c \
	ib_uverbs_main.c \
	ib_uverbs_marshall.c \
	ib_uverbs_std_types.c \
	ib_uverbs_std_types_async_fd.c \
	ib_uverbs_std_types_counters.c \
	ib_uverbs_std_types_cq.c \
	ib_uverbs_std_types_device.c \
	ib_uverbs_std_types_dm.c \
	ib_uverbs_std_types_flow_action.c \
	ib_uverbs_std_types_mr.c \
	ib_uverbs_uapi.c \
	ib_verbs.c

SRCS+=	${LINUXKPI_GENSRCS}
SRCS+=	opt_inet.h opt_inet6.h

CFLAGS+= -I${SRCTOP}/sys/ofed/include
CFLAGS+= -I${SRCTOP}/sys/ofed/include/uapi
CFLAGS+= -I${SRCTOP}/sys/compat/linuxkpi/common/include
CFLAGS+= -DINET6 -DINET -DCONFIG_INFINIBAND_USER_MEM

.include <bsd.kmod.mk>

CWARNFLAGS+= -Wno-cast-qual -Wno-pointer-arith -Wno-redundant-decls
