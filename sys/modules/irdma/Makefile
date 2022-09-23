.include <bsd.own.mk>

OFED_INC_DIR = ${.CURDIR}/../../ofed/include
ICE_DIR = ${.CURDIR}/../../dev/ice
.PATH:	${.CURDIR}/../../dev/irdma

KMOD=	irdma
SRCS=	icrdma.c
SRCS+=	fbsd_kcompat.c irdma_hw.c irdma_verbs.c irdma_utils.c
SRCS+=	irdma_cm.c irdma_kcompat.c
SRCS+=  irdma_if.h irdma_di_if.h ice_rdma.h vnode_if.h device_if.h bus_if.h opt_inet.h pci_if.h opt_inet6.h
# Shared source
SRCS+=	irdma_ctrl.c irdma_hmc.c icrdma_hw.c irdma_pble.c irdma_puda.c irdma_uda.c irdma_uk.c irdma_ws.c

CFLAGS+= -I${ICE_DIR} -I${OFED_INC_DIR}
CFLAGS+= -I${OFED_INC_DIR}/uapi
CFLAGS+= ${LINUXKPI_INCLUDES}
CFLAGS+= -DCONFIG_INFINIBAND_USER_MEM

ice_rdma.h:
	cp $(ICE_DIR)/ice_rdma.h .

.include <bsd.kmod.mk>
