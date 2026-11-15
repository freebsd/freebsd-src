.PATH: ${SRCTOP}/sys/dev/asmc

KMOD=	asmc
SRCS=	asmc.c asmc_pio.c asmc_sysctl.c asmcmmio.c opt_acpi.h opt_asmc.h
SRCS+=	acpi_if.h backlight_if.h bus_if.h device_if.h

.if defined(ASMC_DEBUG)
SRCS+=	asmc_debug.c
.if !defined(KERNBUILDDIR)
opt_asmc.h:
	echo "#define ASMC_DEBUG 1" > ${.TARGET}
.endif
.endif

.include <bsd.kmod.mk>
