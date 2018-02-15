# $FreeBSD$

.include <bsd.init.mk>

LIB=		zfsboot

.PATH:		${ZFSSRC}
SRCS+=		zfs.c skein.c skein_block.c
# Do not unroll skein loops, reduce code size
CFLAGS+=	-DSKEIN_LOOP=111
.PATH:		${SYSDIR}/crypto/skein

CFLAGS+=	-DBOOTPROG=\"zfsloader\"
CFLAGS+=	-I${LDRSRC}
CFLAGS+=	-I${SYSDIR}/cddl/boot/zfs
CFLAGS+=	-I${SYSDIR}/crypto/skein

CFLAGS+=	-Wformat -Wall

.include <bsd.stand.mk>
.include <bsd.lib.mk>
