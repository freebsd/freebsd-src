# $FreeBSD$

.PATH:		${.CURDIR}/../../../i386/acpica

KMOD=		acpi_asus
CFLAGS+=	-I${.CURDIR}/../../../contrib/dev/acpica
SRCS=		acpi_asus.c opt_acpi.h device_if.h bus_if.h

.include <bsd.kmod.mk>
