
.PATH:	${SRCTOP}/sys/dev/iicbus/adc
KMOD=	pcf8591
SRCS=	pcf8591.c

SRCS+=	\
	bus_if.h \
	device_if.h \
	iicbus_if.h \
	opt_platform.h

.if !empty(OPT_FDT)
SRCS+=	ofw_bus_if.h
.endif

.include <bsd.kmod.mk>
