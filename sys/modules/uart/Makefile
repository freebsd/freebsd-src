# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/uart

.if ${MACHINE_ARCH} == "sparc64"
uart_bus_ebus=	uart_bus_ebus.c
ofw_bus_if=	ofw_bus_if.h
ofw_pci_if=	ofw_pci_if.h
opt_isa=	opt_isa.h
.endif

KMOD=	uart
SRCS=	uart_bus_acpi.c ${uart_bus_ebus} uart_bus_isa.c uart_bus_pccard.c \
	uart_bus_pci.c uart_bus_puc.c uart_core.c uart_cpu_${MACHINE}.c \
	uart_dbg.c uart_dev_i8251.c uart_dev_ns8250.c uart_dev_sab82532.c \
	uart_dev_z8530.c uart_if.c uart_subr.c uart_tty.c
SRCS+=	bus_if.h card_if.h device_if.h isa_if.h ${ofw_bus_if} ${ofw_pci_if} \
	${opt_isa} pci_if.h power_if.h uart_if.h pccarddevs.h

opt_isa.h:
	echo "#define DEV_ISA 1" > ${.TARGET}

.include <bsd.kmod.mk>
