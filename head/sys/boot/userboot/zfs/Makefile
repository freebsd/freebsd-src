# $FreeBSD$

S=		${.CURDIR}/../../zfs

.PATH:		${S} ${.CURDIR}/../../../crypto/skein
LIB=		zfsboot
INTERNALLIB=

SRCS+=		zfs.c skein.c skein_block.c

CFLAGS+=	-I${.CURDIR}/../../common -I${.CURDIR}/../../.. -I.
CFLAGS+=	-I${.CURDIR}/../../../../lib/libstand
CFLAGS+=	-I${.CURDIR}/../../../cddl/boot/zfs
CFLAGS+=	-I${.CURDIR}/../../../crypto/skein

CFLAGS+=	-ffreestanding -fPIC
CFLAGS+=	-Wformat -Wall

.include <bsd.lib.mk>
