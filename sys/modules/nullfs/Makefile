# $FreeBSD: src/sys/modules/nullfs/Makefile,v 1.10.2.1 2003/08/13 20:44:51 luoqi Exp $

.PATH:	${.CURDIR}/../../miscfs/nullfs
KMOD=	null
SRCS=	vnode_if.h opt_ddb.h \
	null_subr.c null_vfsops.c null_vnops.c
NOMAN=

.include <bsd.kmod.mk>
