#	$FreeBSD$

.PATH:	${SRCTOP}/sys/dev/acpica
.if ${TARGET_ARCH} == aarch64
CFLAGS += -DINTRNG
.endif
KMOD=	acpi_ged
SRCS=	acpi_ged.c
SRCS+=	opt_acpi.h opt_evdev.h acpi_if.h bus_if.h device_if.h

.include <bsd.kmod.mk>
