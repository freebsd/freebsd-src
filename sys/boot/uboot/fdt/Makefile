# $FreeBSD$

.include <src.opts.mk>

.PATH: ${.CURDIR}/../../common

LIB=		uboot_fdt
INTERNALLIB=
WARNS?=		2

SRCS=		uboot_fdt.c

CFLAGS+=	-ffreestanding -msoft-float

CFLAGS+=	-I${.CURDIR}/../../../../lib/libstand/

# U-Boot library headers
CFLAGS+=	-I${.CURDIR}/../lib

# libfdt headers
CFLAGS+=	-I${.CURDIR}/../../fdt

# Pick up the bootstrap header for some interface items
CFLAGS+=	-I${.CURDIR}/../../common -I${.CURDIR}/../../.. -I.

machine:
	ln -sf ${.CURDIR}/../../../${MACHINE_CPUARCH}/include machine

CLEANFILES+=	machine

.include <bsd.stand.mk>
.include <bsd.lib.mk>

beforedepend ${OBJS}: machine
