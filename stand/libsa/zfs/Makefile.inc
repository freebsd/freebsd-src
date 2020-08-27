# $FreeBSD$

.PATH:		${ZFSSRC}
SRCS+=		zfs.c nvlist.c skein.c skein_block.c list.c
.PATH:		${SYSDIR}/crypto/skein
.PATH:		${SYSDIR}/cddl/contrib/opensolaris/uts/common/os

CFLAGS+=	-I${LDRSRC}
CFLAGS+=	-I${SYSDIR}/cddl/boot/zfs
CFLAGS+=	-I${SYSDIR}/cddl/contrib/opensolaris/uts/common
CFLAGS+=	-I${SYSDIR}/crypto/skein

# Do not unroll skein loops, reduce code size
CFLAGS.skein_block.c+=	-DSKEIN_LOOP=111
CFLAGS.zfs.c+=	-I${SRCTOP}/sys/cddl/contrib/opensolaris/common/lz4

CFLAGS+=	-Wformat -Wall
