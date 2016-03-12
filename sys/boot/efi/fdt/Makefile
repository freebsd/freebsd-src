# $FreeBSD$

.include <src.opts.mk>

.PATH: ${.CURDIR}/../../common

LIB=		efi_fdt
INTERNALLIB=
WARNS?=		6

SRCS=		efi_fdt.c

CFLAGS+=	-ffreestanding -msoft-float
.if ${MACHINE_CPUARCH} == "aarch64"
CFLAGS+=	-mgeneral-regs-only
.endif

CFLAGS+=	-I${.CURDIR}/../../../../lib/libstand/

# EFI library headers
CFLAGS+=	-I${.CURDIR}/../include
CFLAGS+=	-I${.CURDIR}/../include/${MACHINE}

# libfdt headers
CFLAGS+=	-I${.CURDIR}/../../fdt

# Pick up the bootstrap header for some interface items
CFLAGS+=	-I${.CURDIR}/../../common -I${.CURDIR}/../../.. -I.

machine:
	ln -sf ${.CURDIR}/../../../${MACHINE}/include machine

CLEANFILES+=	machine

.include <bsd.lib.mk>

beforedepend ${OBJS}: machine
