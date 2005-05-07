# $FreeBSD: src/sys/modules/acpi/acpi_toshiba/Makefile,v 1.3 2004/08/03 21:27:43 njl Exp $

.PATH:	${.CURDIR}/../../../i386/acpica

KMOD=	acpi_toshiba
WARNS?=	2
CFLAGS+= -I${.CURDIR}/../../../contrib/dev/acpica
SRCS=	acpi_toshiba.c opt_acpi.h acpi_if.h bus_if.h device_if.h

.include <bsd.kmod.mk>
