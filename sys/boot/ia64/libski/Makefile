# $FreeBSD$

LIB=		ski
INTERNALLIB=	true

SRCS=	skiconsole.c time.c copy.c devicename.c module.c exit.c
SRCS+=	delay.c skifs.c elf_freebsd.c bootinfo.c ssc.c
SRCS+=	acpi_stub.c efi_stub.c pal_stub.S sal_stub.c

CFLAGS+=	-I${.CURDIR}/../../../../lib/libstand
CFLAGS+=	-I${.CURDIR}/../../efi/include
CFLAGS+=	-I${.CURDIR}/../../efi/include/${MACHINE_ARCH}

# Pick up the bootstrap header for some interface items
CFLAGS+=	-I${.CURDIR}/../../common -I${.CURDIR}/../../..

.if defined(BOOT_DISK_DEBUG)
# Make the disk code more talkative
CFLAGS+= -DDISK_DEBUG
.endif

.include <bsd.lib.mk>
