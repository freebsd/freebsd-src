.PATH:	${SRCTOP}/sys/dev/iicbus/rtc/
KMOD	= rs5c372a
SRCS	= rs5c372a.c

# Generated files...
SRCS+=	\
	bus_if.h \
	clock_if.h \
	device_if.h \
	iicbus_if.h \
	opt_platform.h \
	ofw_bus_if.h \

.include <bsd.kmod.mk>
