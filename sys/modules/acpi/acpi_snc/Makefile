# $FreeBSD$

.PATH:		${.CURDIR}/../../../dev/acpica

KMOD=		acpi_snc
CFLAGS+=	-I@/contrib/dev/acpica
SRCS=		acpi_snc.c opt_acpi.h device_if.h bus_if.h acpi_if.h

.include <bsd.kmod.mk>
