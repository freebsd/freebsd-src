# $FreeBSD$

.PATH: ${.CURDIR}/../../../${MACHINE_ARCH}/${MACHINE_ARCH}

LIB=		efi
INTERNALLIB=

SRCS=	libefi.c efi_console.c time.c copy.c devicename.c module.c
SRCS+=	delay.c efifs.c efinet.c elf_freebsd.c bootinfo.c pal.S

.if ${MACHINE_ARCH} == "ia64"
SRCS+=	efifpswa.c
.endif

CFLAGS+=	-I${.CURDIR}/../include
CFLAGS+=	-I${.CURDIR}/../include/${MACHINE_ARCH}
CFLAGS+=	-I${.CURDIR}/../../../../lib/libstand

# Pick up the bootstrap header for some interface items
CFLAGS+=	-I${.CURDIR}/../../common

.if defined(BOOT_DISK_DEBUG)
# Make the disk code more talkative
CFLAGS+= -DDISK_DEBUG
.endif

.include <bsd.lib.mk>
