.PATH:  ${SRCTOP}/sys/ofed/drivers/infiniband/ulp/ipoib

KMOD=	ipoib
SRCS= 	ipoib_cm.c ipoib_ib.c ipoib_main.c \
	ipoib_multicast.c ipoib_verbs.c
SRCS+=	${LINUXKPI_GENSRCS}
SRCS+=	opt_inet.h opt_inet6.h opt_ofed.h

CFLAGS+= -I${SRCTOP}/sys/ofed/drivers/infiniband/ulp/ipoib
CFLAGS+= -I${SRCTOP}/sys/ofed/include
CFLAGS+= -I${SRCTOP}/sys/ofed/include/uapi
CFLAGS+= ${LINUXKPI_INCLUDES}

.include <bsd.kmod.mk>

CFLAGS+= -Wno-cast-qual -Wno-pointer-arith
