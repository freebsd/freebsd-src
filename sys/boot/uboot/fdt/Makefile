# $FreeBSD$

.include <bsd.init.mk>

.PATH: ${LDRSRC}

LIB=		uboot_fdt
INTERNALLIB=
WARNS?=		2

SRCS=		uboot_fdt.c

CFLAGS+=	-ffreestanding -msoft-float

# U-Boot library headers
CFLAGS+=	-I${UBOOTSRC}/lib

# libfdt headers
CFLAGS+=	-I${FDTSRC}

# Pick up the bootstrap header for some interface items
CFLAGS+=	-I${LDRSRC} -I${SYSDIR} -I.

.include <bsd.stand.mk>
.include <bsd.lib.mk>
