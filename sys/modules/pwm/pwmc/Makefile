
.PATH:	${SRCTOP}/sys/dev/pwm
KMOD=	pwmc
SRCS=	pwmc.c

# Generated files...
SRCS+=	\
	bus_if.h \
	device_if.h \
	opt_platform.h \
	pwmbus_if.h \

.if !empty(OPT_FDT)
SRCS+=	ofw_bus_if.h
.endif

.include <bsd.kmod.mk>
