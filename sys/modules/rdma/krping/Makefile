# $FreeBSD$
.PATH: ${SRCTOP}/sys/contrib/rdma/krping

KMOD= krping
SRCS= krping.c krping_dev.c getopt.c
SRCS+=	${LINUXKPI_GENSRCS}
SRCS+=  opt_inet.h opt_inet6.h
CFLAGS+= -I${SRCTOP}/sys/ofed/include
CFLAGS+= -I${SRCTOP}/sys/ofed/include/uapi
CFLAGS+= ${LINUXKPI_INCLUDES}

.include <bsd.kmod.mk>
