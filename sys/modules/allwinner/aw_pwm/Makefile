
.PATH: ${SRCTOP}/sys/dev/pwm/controller/allwinner

KMOD=	aw_pwm
SRCS=	aw_pwm.c

SRCS+=	\
	bus_if.h \
	clknode_if.h \
	device_if.h \
	ofw_bus_if.h \
	pwmbus_if.h

.include <bsd.kmod.mk>
