# $FreeBSD: src/sys/modules/msdos/Makefile,v 1.10.2.1 2000/06/23 11:20:52 nyan Exp $

.PATH:	${.CURDIR}/../../msdosfs
KMOD=	msdos
SRCS=	vnode_if.h \
	msdosfs_conv.c msdosfs_denode.c msdosfs_fat.c msdosfs_lookup.c \
	msdosfs_vfsops.c msdosfs_vnops.c
NOMAN=

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.kmod.mk>
