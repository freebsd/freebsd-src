# $FreeBSD$

.PATH:		${.CURDIR}/../../../dev/acpica
CFLAGS+=	-I${.CURDIR}/../../../contrib/dev/acpica

KMOD=		acpi_perf
WARNS?=		2
SRCS=		acpi_perf.c acpi_throttle.c
SRCS+=		acpi_if.h bus_if.h cpufreq_if.h device_if.h opt_acpi.h \
		pci_if.h

.include <bsd.kmod.mk>
