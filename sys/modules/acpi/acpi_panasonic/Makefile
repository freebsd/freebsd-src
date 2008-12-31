# $FreeBSD: src/sys/modules/acpi/acpi_panasonic/Makefile,v 1.4.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH:	${.CURDIR}/../../../dev/acpi_support

KMOD=	acpi_panasonic
CFLAGS+= -I${.CURDIR}/../../../contrib/dev/acpica
SRCS=	acpi_panasonic.c opt_acpi.h acpi_if.h bus_if.h device_if.h

.include <bsd.kmod.mk>
