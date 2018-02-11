# $FreeBSD$

S=		${.CURDIR}/../../zfs

.PATH:		${S} ${.CURDIR}/../../../crypto/skein
LIB=		zfsboot
INTERNALLIB=

SRCS+=		zfs.c skein.c skein_block.c

# Do not unroll skein loops, reduce code size
CFLAGS+=	-DSKEIN_LOOP=111

CFLAGS+=	-I${.CURDIR}/../../common -I${.CURDIR}/../../.. -I.
CFLAGS+=	-I${SASRC}
CFLAGS+=	-I${.CURDIR}/../../../cddl/boot/zfs
CFLAGS+=	-I${.CURDIR}/../../../crypto/skein

CFLAGS+=	-ffreestanding -fPIC
CFLAGS+=	-Wformat -Wall

.include <bsd.lib.mk>
