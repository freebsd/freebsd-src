
.PATH:		${SRCTOP}/sys/dev/iicbus/mux

KMOD=	iicmux
SRCS=   iicmux.c iicmux_if.c

SRCS+=	\
	bus_if.h \
	device_if.h \
	iicbus_if.h \
	iicmux_if.h \
	opt_platform.h \

.if !empty(OPT_FDT)
SRCS+=	ofw_bus_if.h
.endif

EXPORT_SYMS=	YES

.include <bsd.kmod.mk>
