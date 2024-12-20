.PATH:	${SRCTOP}/sys/dev/iicbus/rtc/
KMOD	= rx8803
SRCS	= rx8803.c

# Generated files...
SRCS+=	\
	bus_if.h \
	clock_if.h \
	device_if.h \
	iicbus_if.h \
	opt_platform.h \
	ofw_bus_if.h \

.include <bsd.kmod.mk>
