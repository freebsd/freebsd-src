.PATH:	${SRCTOP}/sys/dev/spibus

KMOD=	spibus
SRCS=	spibus.c

.if !empty(OPT_FDT)
SRCS+=	ofw_spibus.c ofw_bus_if.h
.endif

.if ${MACHINE_CPUARCH} == "aarch64" || ${MACHINE_CPUARCH} == "amd64" || \
    ${MACHINE_CPUARCH} == "i386"
SRCS+= acpi_spibus.c opt_acpi.h acpi_if.h
.endif

# Generated files...
SRCS+=	\
	bus_if.h \
	device_if.h \
	opt_platform.h \
	spibus_if.c \
	spibus_if.h \

EXPORT_SYMS=	YES

.include <bsd.kmod.mk>
