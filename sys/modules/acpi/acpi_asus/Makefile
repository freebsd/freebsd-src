# $FreeBSD$

.PATH:	${.CURDIR}/../../../i386/acpica

KMOD=	acpi_asus
WARNS?=	2
CFLAGS+= -I${.CURDIR}/../../../contrib/dev/acpica
SRCS=	acpi_asus.c opt_acpi.h acpi_if.h bus_if.h device_if.h

.include <bsd.kmod.mk>
