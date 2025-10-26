.PATH: ${SRCTOP}/sys/dev/thunderbolt

KMOD=	tb
SRCS=	nhi_pci.c nhi.c tb_pcib.c tb_acpi_pcib.c tb_debug.c nhi_wmi.c
SRCS+=	router.c hcm.c tb_dev.c
SRCS+=	opt_thunderbolt.h
SRCS+=	device_if.h bus_if.h pci_if.h pcib_if.h tb_if.c tb_if.h
SRCS+=	opt_acpi.h acpi_if.h acpi_wmi_if.h

opt_thunderbolt.h:
	echo "#define THUNDERBOLT_DEBUG 1" > ${.TARGET}

.include <bsd.kmod.mk>
