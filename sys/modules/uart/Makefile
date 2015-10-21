# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/uart

.if ${MACHINE_CPUARCH} == "sparc64"
uart_bus_ebus=	uart_bus_ebus.c
.endif

.if ${MACHINE_CPUARCH} == "arm"
uart_dev_lpc=	uart_dev_lpc.c
.endif

.if ${MACHINE_CPUARCH} == "aarch64" || ${MACHINE_CPUARCH} == "arm" || \
    ${MACHINE_CPUARCH} == "sparc64" || ${MACHINE_CPUARCH} == "powerpc"
ofw_bus_if=	ofw_bus_if.h
.endif

.if ${MACHINE} == "i386" || ${MACHINE} == "amd64"
_uart_cpu=uart_cpu_x86.c
.else
_uart_cpu=uart_cpu_${MACHINE}.c
.endif
.if exists(${.CURDIR:H:H}/dev/uart/${_uart_cpu})
uart_cpu_machine= ${_uart_cpu}
.endif

KMOD=	uart
SRCS=	uart_bus_acpi.c ${uart_bus_ebus} uart_bus_isa.c uart_bus_pccard.c \
	uart_bus_pci.c uart_bus_puc.c uart_bus_scc.c \
	uart_core.c ${uart_cpu_machine} uart_dbg.c \
	${uart_dev_lpc} uart_dev_ns8250.c uart_dev_quicc.c uart_dev_sab82532.c \
	uart_dev_z8530.c \
	uart_if.c uart_if.h uart_subr.c uart_tty.c

SRCS+=	bus_if.h card_if.h device_if.h isa_if.h ${ofw_bus_if} pci_if.h \
	power_if.h pccarddevs.h serdev_if.h
SRCS+=	opt_platform.h

.include <bsd.kmod.mk>
