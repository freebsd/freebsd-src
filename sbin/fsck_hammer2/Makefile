# $FreeBSD$

PACKAGE=runtime
PROG=	fsck_hammer2
SRCS=	fsck_hammer2.c test.c ondisk.c subs.c xxhash.c gsb_crc32.c
MAN=	fsck_hammer2.8

.PATH:	${SRCTOP}/sbin/hammer2 ${SRCTOP}/sys/fs/hammer2/xxhash ${SRCTOP}/sys/libkern

WARNS?=	3

CFLAGS+=	-DXXH_NAMESPACE=h2_
CFLAGS+=	-I${SRCTOP}/sys
CFLAGS+=	-I${SRCTOP}/sbin/hammer2

LIBADD=		md

.include <bsd.prog.mk>
