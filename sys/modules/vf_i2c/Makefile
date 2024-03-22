.PATH: ${SRCTOP}/sys/dev/iicbus/controller/vybrid

KMOD=	vf_i2c
SRCS=	vf_i2c.c vf_i2c.h
SRCS+=	device_if.h bus_if.h iicbus_if.h
SRCS+=	opt_acpi.h opt_platform.h

SRCS.DEV_ACPI=	vf_i2c_acpi.c acpi_if.h

.if !empty(OPT_FDT)
SRCS+=	vf_i2c_fdt.c ofw_bus_if.h clknode_if.h
.endif

.include <bsd.kmod.mk>
