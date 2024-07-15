CXGBE=	${SRCTOP}/sys/dev/cxgbe
.PATH: ${CXGBE}/iw_cxgbe

KMOD=	iw_cxgbe
SRCS+=	cm.c
SRCS+=	cq.c
SRCS+=	device.c
SRCS+=	ev.c
SRCS+=	id_table.c
SRCS+=	mem.c
SRCS+=	provider.c
SRCS+=	qp.c
SRCS+=	resource.c
SRCS+=	${LINUXKPI_GENSRCS}
SRCS+=	opt_inet.h opt_inet6.h opt_kern_tls.h opt_ktr.h opt_ofed.h

CFLAGS+= -I${CXGBE} -I${SRCTOP}/sys/ofed/include -DLINUX_TYPES_DEFINED
CFLAGS+= -I${SRCTOP}/sys/ofed/include/uapi
CFLAGS+= ${LINUXKPI_INCLUDES}
CFLAGS+= -DCONFIG_INFINIBAND_USER_MEM
CFLAGS+= -DINET6 -DINET

.include <bsd.kmod.mk>
