# $FreeBSD: src/sys/modules/union/Makefile,v 1.7 1999/12/12 20:34:11 peter Exp $

.PATH:	${.CURDIR}/../../miscfs/union
KMOD=	union
SRCS=	vnode_if.h \
	union_subr.c union_vfsops.c union_vnops.c
NOMAN=

.include <bsd.kmod.mk>
