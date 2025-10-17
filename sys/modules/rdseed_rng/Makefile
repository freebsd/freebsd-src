.PATH: ${SRCTOP}/sys/dev/random

KMOD=	rdseed_rng
SRCS=	rdseed.c
SRCS+=	bus_if.h device_if.h

CFLAGS+= -I${SRCTOP}/sys

.include <bsd.kmod.mk>
