# $FreeBSD: src/sys/modules/acpi/acpi_fujitsu/Makefile,v 1.2.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

.PATH:	${.CURDIR}/../../../dev/acpi_support

KMOD=	acpi_fujitsu
CFLAGS+=-I${.CURDIR}/../../../dev/acpi_support
SRCS=	acpi_fujitsu.c opt_acpi.h acpi_if.h device_if.h bus_if.h

.include <bsd.kmod.mk>
