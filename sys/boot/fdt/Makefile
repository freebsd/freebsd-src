# $FreeBSD$

.PATH:		${.CURDIR}/../../contrib/libfdt/

LIB=		fdt
INTERNALLIB=

# Vendor sources of libfdt.
SRCS+=		fdt.c fdt_ro.c fdt_wip.c fdt_sw.c fdt_rw.c fdt_strerror.c

# Loader's fdt commands extension sources.
SRCS+=		fdt_loader_cmd.c

CFLAGS+=	-I${.CURDIR}/../../contrib/libfdt/ -I${.CURDIR}/../common/ \
		-I${.CURDIR}/../uboot/lib

CFLAGS+=	-ffreestanding

.if ${MACHINE_ARCH} == "powerpc" || ${MACHINE_ARCH} == "arm"
CFLAGS+=	-msoft-float
.endif

CFLAGS+=	-Wformat -Wall

.include <bsd.lib.mk>
