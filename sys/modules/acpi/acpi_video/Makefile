#	$FreeBSD: src/sys/modules/acpi/acpi_video/Makefile,v 1.7.12.1 2010/12/21 17:10:29 kensmith Exp $

.PATH:	${.CURDIR}/../../../dev/acpica

KMOD=	acpi_video
CFLAGS+= -I${.CURDIR}/../../../contrib/dev/acpica
SRCS=	acpi_video.c
SRCS+=	opt_acpi.h acpi_if.h bus_if.h device_if.h

.include <bsd.kmod.mk>
