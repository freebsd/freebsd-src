# $FreeBSD$

.PATH: ${.CURDIR}/../../../${MACHINE_ARCH}/${MACHINE_ARCH}

LIB=	efi
INTERNALLIB=

SRCS=	bootinfo.c copy.c delay.c devicename.c efi_console.c efifs.c efinet.c \
	elf_freebsd.c libefi.c module.c time.c

.if ${MACHINE_ARCH} == "ia64"
SRCS+=	efifpswa.c pal.S
.endif

CFLAGS+= -I${.CURDIR}/../include
CFLAGS+= -I${.CURDIR}/../include/${MACHINE_ARCH}
CFLAGS+= -I${.CURDIR}/../../../../lib/libstand

# Pick up the bootstrap header for some interface items
CFLAGS+= -I${.CURDIR}/../../common

# Make the disk code more talkative
.if defined(BOOT_DISK_DEBUG)
CFLAGS+= -DDISK_DEBUG
.endif

.include <bsd.lib.mk>
