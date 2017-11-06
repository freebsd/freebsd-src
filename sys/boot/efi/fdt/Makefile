# $FreeBSD$

.include <bsd.init.mk>

.PATH: ${LDRSRC}

LIB=		efi_fdt
INTERNALLIB=
WARNS?=		6

SRCS=		efi_fdt.c

CFLAGS+=	-ffreestanding
.if ${MACHINE_CPUARCH} == "aarch64"
CFLAGS+=	-mgeneral-regs-only
.else
CFLAGS+=	-msoft-float
.endif

# EFI library headers
CFLAGS+=	-I${EFISRC}/include
CFLAGS+=	-I${EFISRC}/include/${MACHINE}

# libfdt headers
CFLAGS+=	-I${FDTSRC}

# Pick up the bootstrap header for some interface items
CFLAGS+=	-I${LDRSRC} -I${SYSDIR} -I.

.include <bsd.lib.mk>
