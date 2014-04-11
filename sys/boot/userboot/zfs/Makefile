# $FreeBSD$

S=		${.CURDIR}/../../zfs

.PATH:		${S}
LIB=		zfsboot
INTERNALLIB=

SRCS+=		zfs.c

CFLAGS+=	-I${.CURDIR}/../../common -I${.CURDIR}/../../.. -I.
CFLAGS+=	-I${.CURDIR}/../../../../lib/libstand
CFLAGS+=	-I${.CURDIR}/../../../cddl/boot/zfs

CFLAGS+=	-ffreestanding -fPIC
CFLAGS+=	-Wformat -Wall

.include <bsd.lib.mk>
