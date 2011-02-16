# $FreeBSD: src/sys/modules/acpi/acpi_panasonic/Makefile,v 1.4.12.1 2010/12/21 17:10:29 kensmith Exp $

.PATH:	${.CURDIR}/../../../dev/acpi_support

KMOD=	acpi_panasonic
CFLAGS+= -I${.CURDIR}/../../../contrib/dev/acpica
SRCS=	acpi_panasonic.c opt_acpi.h acpi_if.h bus_if.h device_if.h

.include <bsd.kmod.mk>
