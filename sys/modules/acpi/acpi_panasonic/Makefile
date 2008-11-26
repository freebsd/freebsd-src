# $FreeBSD: src/sys/modules/acpi/acpi_panasonic/Makefile,v 1.3.12.1 2008/10/02 02:57:24 kensmith Exp $

.PATH:	${.CURDIR}/../../../dev/acpi_support

KMOD=	acpi_panasonic
WARNS?=	2
CFLAGS+= -I${.CURDIR}/../../../contrib/dev/acpica
SRCS=	acpi_panasonic.c opt_acpi.h acpi_if.h bus_if.h device_if.h

.include <bsd.kmod.mk>
