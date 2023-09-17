
.PATH: ${SRCTOP}/sys/dev/mmc/host/

KMOD=	rk_dwmmc
SRCS=	dwmmc.c dwmmc_rockchip.c

SRCS+=	\
	bus_if.h \
	clknode_if.h \
	device_if.h \
	ofw_bus_if.h

.include <bsd.kmod.mk>
