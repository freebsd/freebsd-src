# $FreeBSD$

LIB=		efi
NOPIC=		true
NOPROFILE=	true
INTERNALLIB=	true
INTERNALSTATICLIB=	true

SRCS=	libefi.c efi_console.c time.c copy.c devicename.c module.c exit.c
SRCS+=	delay.c

CFLAGS+=	-fpic
CFLAGS+=	-I${.CURDIR}/../include
CFLAGS+=	-I${.CURDIR}/../include/${MACHINE_ARCH}
CFLAGS+=	-I${.CURDIR}/../../../../lib/libstand/

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
