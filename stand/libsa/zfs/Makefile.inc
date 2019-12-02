# $FreeBSD$

.PATH:		${ZFSSRC}
SRCS+=		zfs.c skein.c skein_block.c list.c
# Do not unroll skein loops, reduce code size
CFLAGS+=	-DSKEIN_LOOP=111
.PATH:		${SYSDIR}/crypto/skein
.PATH:		${SYSDIR}/cddl/contrib/opensolaris/uts/common/os

CFLAGS+=	-I${LDRSRC}
CFLAGS+=	-I${SYSDIR}/cddl/boot/zfs
CFLAGS+=	-I${SYSDIR}/cddl/contrib/opensolaris/uts/common
CFLAGS+=	-I${SYSDIR}/crypto/skein

CFLAGS.zfs.c+=	-I${SRCTOP}/sys/cddl/contrib/opensolaris/common/lz4

CFLAGS+=	-Wformat -Wall
