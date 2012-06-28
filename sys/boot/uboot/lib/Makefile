# $FreeBSD$

.PATH: ${.CURDIR}/../../common

LIB=		uboot
INTERNALLIB=
WARNS?=		2

SRCS=	crc32.c console.c copy.c devicename.c elf_freebsd.c glue.c
SRCS+=	module.c net.c reboot.c storage.c time.c

CFLAGS+=	-ffreestanding -msoft-float

CFLAGS+=	-I${.CURDIR}/../../../../lib/libstand/

# Pick up FDT includes
CFLAGS+=	-I${.CURDIR}/../../../../sys/contrib/libfdt/

# Pick up the bootstrap header for some interface items
CFLAGS+=	-I${.CURDIR}/../../common -I${.CURDIR}/../../.. -I.

.ifdef(BOOT_DISK_DEBUG)
# Make the disk code more talkative
CFLAGS+= -DDISK_DEBUG
.endif

machine:
	ln -sf ${.CURDIR}/../../../${MACHINE_CPUARCH}/include machine

CLEANFILES+=	machine

.include <bsd.lib.mk>

beforedepend ${OBJS}: machine
