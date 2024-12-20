.PATH:		${SRCTOP}/sys/dev/iicbus/mux

KMOD=	iic_gpiomux
SRCS=	iic_gpiomux.c

SRCS+=	\
	bus_if.h \
	device_if.h \
	gpio_if.h \
	iicbus_if.h \
	iicmux_if.h \
	opt_platform.h \

.if !empty(OPT_FDT)
SRCS+=	ofw_bus_if.h
.endif

.include <bsd.kmod.mk>
