.PATH:  ${SRCTOP}/sys/dev/etherswitch

KMOD    = miiproxy

SRCS=	miiproxy.c
SRCS+=	bus_if.h mdio_if.h miibus_if.h opt_platform.h
CFLAGS+= -I${SRCTOP}/sys/dev/etherswitch

.include <bsd.kmod.mk>
