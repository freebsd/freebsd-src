# $FreeBSD$

.PATH:	${.CURDIR}/../../miscfs/union
KMOD=	union
SRCS=	vnode_if.h \
	union_subr.c union_vfsops.c union_vnops.c
NOMAN=
CFLAGS+=	-DUNION

.include <bsd.kmod.mk>
