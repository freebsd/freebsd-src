.PATH: ${SRCTOP}/sys/dev/neta

CFLAGS+= -DFDT	

KMOD=	if_mvneta
SRCS=	if_mvneta.c if_mvneta_fdt.c if_mvnetavar.h
SRCS+=	bus_if.h device_if.h mdio_if.h miibus_if.h ofw_bus_if.h clknode_if.h opt_platform.h
.include <bsd.kmod.mk>
