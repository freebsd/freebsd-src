.PATH:  ${SRCTOP}/sys/dev/etherswitch

KMOD    = miiproxy

SRCS=	miiproxy.c
SRCS+=	mdio_if.h miibus_if.h
CFLAGS+= -I${SRCTOP}/sys/dev/etherswitch

.include <bsd.kmod.mk>
