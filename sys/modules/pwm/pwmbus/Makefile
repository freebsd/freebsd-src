.PATH:	${SRCTOP}/sys/dev/pwm

KMOD=	pwmbus
SRCS=	pwmbus.c

.if !empty(OPT_FDT)
SRCS+=	ofw_pwm.c ofw_pwmbus.c ofw_bus_if.h
.endif

# Generated files...
SRCS+=	\
	bus_if.h \
	device_if.h \
	opt_platform.h \
	pwmbus_if.c \
	pwmbus_if.h \

EXPORT_SYMS=	YES

.include <bsd.kmod.mk>
