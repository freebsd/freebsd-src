# $FreeBSD$

LIB=		uboot
INTERNALLIB=

SRCS=	devicename.c elf_freebsd.c console.c copy.c disk.c \
	module.c net.c reboot.c time.c glue.c

CFLAGS+=	-I${.CURDIR}/../../../../lib/libstand/

# Pick up the bootstrap header for some interface items
CFLAGS+=	-I${.CURDIR}/../../common -I${.CURDIR}/../../.. -I.

CFLAGS+=	-ffreestanding -msoft-float

.ifdef(BOOT_DISK_DEBUG)
# Make the disk code more talkative
CFLAGS+= -DDISK_DEBUG
.endif

machine:
	ln -sf ${.CURDIR}/../../../${MACHINE_ARCH}/include machine

CLEANFILES+=	machine

.include <bsd.lib.mk>

beforedepend ${OBJS}: machine
