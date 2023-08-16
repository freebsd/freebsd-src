
.PATH:	${SRCTOP}/sys/dev/iicbus

KMOD=	iicbus
SRCS=	\
	bus_if.h \
	device_if.h \
	iic_recover_bus.c \
	iicbus.c \
	iicbus.h \
	iicbus_if.c \
	iicbus_if.h \
	iiconf.c \
	iiconf.h \
	opt_platform.h \

.if ${MACHINE_CPUARCH} == "aarch64" || ${MACHINE_CPUARCH} == "amd64" || \
    ${MACHINE_CPUARCH} == "i386"
SRCS+=	acpi_iicbus.c opt_acpi.h acpi_if.h
.endif

.if !empty(OPT_FDT)
SRCS+=	ofw_iicbus.c ofw_iicbus_if.h ofw_bus_if.h
.endif

EXPORT_SYMS=	YES

.include <bsd.kmod.mk>
