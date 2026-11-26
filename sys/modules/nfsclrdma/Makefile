.PATH:	${SRCTOP}/sys/ofed/drivers/infiniband/ulp/nfsclrdma

KMOD=nfsclrdma
SRCS= \
	clnt_rdma.c \
	xprt_verbs.c
SRCS+=	${LINUXKPI_GENSRCS}
SRCS+= 	opt_inet.h opt_inet6.h

CFLAGS+= -I${SRCTOP}/sys/ofed/include
CFLAGS+= -I${SRCTOP}/sys/ofed/include/uapi
CFLAGS+= ${LINUXKPI_INCLUDES}

.include <bsd.kmod.mk>

CFLAGS+= -Wno-cast-qual -Wno-pointer-arith ${GCC_MS_EXTENSIONS}
