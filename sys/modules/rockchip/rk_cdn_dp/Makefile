.PATH: ${SRCTOP}/sys/arm64/rockchip

KMOD=	rk_cdn_dp
SRCS=	rk_cdn_dp.c
SRCS+=	rk3399_typec_altmode_var.h

SRCS+=	\
	bus_if.h \
	clknode_if.h \
	device_if.h \
	ofw_bus_if.h \
	phynode_if.h \
	syscon_if.h \
	opt_platform.h

.include <bsd.kmod.mk>
