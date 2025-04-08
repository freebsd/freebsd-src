.PATH: ${SRCTOP}/sys/dev/iicbus/controller/qcom

KMOD=   geni_iic

SRCS=   geni_iic.c geni_iic_var.h
SRCS+=  geni_iic_acpi.c acpi_if.h
SRCS+=  device_if.h bus_if.h iicbus_if.h
SRCS+=  opt_acpi.h opt_platform.h

.include <bsd.kmod.mk>
