# $FreeBSD: src/sys/modules/acpi/acpi_panasonic/Makefile,v 1.4.10.1 2010/02/10 00:26:20 kensmith Exp $

.PATH:	${.CURDIR}/../../../dev/acpi_support

KMOD=	acpi_panasonic
CFLAGS+= -I${.CURDIR}/../../../contrib/dev/acpica
SRCS=	acpi_panasonic.c opt_acpi.h acpi_if.h bus_if.h device_if.h

.include <bsd.kmod.mk>
