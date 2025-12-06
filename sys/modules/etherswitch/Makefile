.PATH:  ${SRCTOP}/sys/dev/etherswitch

KMOD    = etherswitch

SRCS=	etherswitch.c
SRCS+=	bus_if.h device_if.h mdio_if.h miibus_if.h etherswitch_if.h etherswitch_if.c
CFLAGS+= -I${SRCTOP}/sys/dev/etherswitch

.include <bsd.kmod.mk>
