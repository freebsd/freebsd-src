.PATH:  ${SRCTOP}/sys/dev/hyperv/hvsock

KMOD=	hv_sock
SRCS=	hv_sock.c
SRCS+=	hv_sock.h
SRCS+=	device_if.h bus_if.h

CFLAGS+= -I${SRCTOP}/sys/dev/hyperv/include	\
	 -I${SRCTOP}/sys/dev/hyperv/vmbus	\
	 -I${SRCTOP}/sys/dev/hyperv/hvsock

.include <bsd.kmod.mk>
