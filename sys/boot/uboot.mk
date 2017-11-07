# $FreeBSD$

SRCS+=	main.c metadata.c

.PATH:		${UBOOTSRC}/common

CFLAGS+=	-I${UBOOTSRC}/common

# U-Boot standalone support library
LIBUBOOT=	${BOOTOBJ}/uboot/lib/libuboot.a
CFLAGS+=	-I${UBOOTSRC}/lib
CFLAGS+=	-I${BOOTOBJ}/uboot/lib

.if ${LOADER_FDT_SUPPORT} == "yes"
CFLAGS+=	-I${FDTSRC}
CFLAGS+=	-I${BOOTOBJ}/fdt
CFLAGS+=	-DLOADER_FDT_SUPPORT
LIBUBOOT_FDT=	${BOOTOBJ}/uboot/fdt/libuboot_fdt.a
LIBFDT=		${BOOTOBJ}/fdt/libfdt.a
.endif
