#	$Id: Makefile,v 1.1 2002/12/27 20:34:35 taku Exp $
#	$FreeBSD$
KMOD=	acpi_video
SRCS=	acpi_video.c
SRCS+=	opt_acpi.h bus_if.h device_if.h
NOMAN=

.PATH: ${.CURDIR}/../../../dev/acpica
CFLAGS+= -I@/contrib/dev/acpica

.include <bsd.kmod.mk>
