# $FreeBSD$

.PATH:		${.CURDIR}/../../../i386/acpica

KMOD=		acpi_toshiba
CFLAGS+=	-I${.CURDIR}/../../../contrib/dev/acpica
SRCS=		acpi_toshiba.c opt_acpi.h device_if.h bus_if.h

.include <bsd.kmod.mk>
