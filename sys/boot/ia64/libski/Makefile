# $FreeBSD$

LIB=		ski
NOPIC=		true
NOPROFILE=	true
INTERNALLIB=	true
INTERNALSTATICLIB=	true

SRCS=	skiconsole.c time.c copy.c devicename.c module.c exit.c
SRCS+=	delay.c skifs.c elf_freebsd.c bootinfo.c ssc.c

CFLAGS+=	-fpic -g
CFLAGS+=	-I${.CURDIR}/../include
CFLAGS+=	-I${.CURDIR}/../include/${MACHINE_ARCH}
CFLAGS+=	-I${.CURDIR}/../../../../lib/libstand/
CFLAGS+=	-I${.CURDIR}/../../efi/include
CFLAGS+=	-I${.CURDIR}/../../efi/include/${MACHINE_ARCH}

# Pick up the bootstrap header for some interface items
CFLAGS+=	-I${.CURDIR}/../../common -I${.CURDIR}/../../.. -I.

.if ${MACHINE_ARCH} == "powerpc"
CFLAGS+=	-msoft-float
.endif

.ifdef(BOOT_DISK_DEBUG)
# Make the disk code more talkative
CFLAGS+= -DDISK_DEBUG
.endif

machine:
	ln -sf ${.CURDIR}/../../../${MACHINE_ARCH}/include machine

CLEANFILES+=	machine

.include <bsd.lib.mk>

beforedepend ${OBJS}: machine
