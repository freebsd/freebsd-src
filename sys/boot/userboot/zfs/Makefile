# $FreeBSD$

.include <bsd.init.mk>

.PATH:		${BOOTSRC}/zfs ${SYSDIR}/crypto/skein
LIB=		zfsboot
INTERNALLIB=

SRCS+=		zfs.c skein.c skein_block.c

# Do not unroll skein loops, reduce code size
CFLAGS+=	-DSKEIN_LOOP=111

CFLAGS+=	-I${LDRSRC} -I${SYSDIR} -I.
CFLAGS+=	-I${SASRC}
CFLAGS+=	-I${SYSDIR}/cddl/boot/zfs
CFLAGS+=	-I${SYSDIR}/crypto/skein

CFLAGS+=	-ffreestanding -fPIC
CFLAGS+=	-Wformat -Wall

.include <bsd.lib.mk>
