# $FreeBSD$

.PATH:		${.CURDIR}/../../../dev/acpica
CFLAGS+=	-I${.CURDIR}/../../../contrib/dev/acpica

KMOD=		acpi_perf
WARNS?=		2
SRCS=		acpi_perf.c
SRCS+=		acpi_if.h bus_if.h cpufreq_if.h device_if.h opt_acpi.h

.include <bsd.kmod.mk>
