.PATH:	${SRCTOP}/sys/dev/iicbus/sensor/
KMOD	= w83793g
SRCS	= w83793g.c

# Generated files...
SRCS+=	\
	bus_if.h \
	clock_if.h \
	device_if.h \
	iicbus_if.h \
	opt_platform.h \
	ofw_bus_if.h \

.include <bsd.kmod.mk>
