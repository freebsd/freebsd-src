# $FreeBSD$

SYSDIR?=	${SRCTOP}/sys

.PATH:		${SYSDIR}/cddl/dev/dtmalloc

KMOD=		dtmalloc
SRCS=		dtmalloc.c
SRCS+=		vnode_if.h

CFLAGS+=	${OPENZFS_CFLAGS}

.include <bsd.kmod.mk>

CFLAGS+=	-include ${SYSDIR}/cddl/compat/opensolaris/sys/debug_compat.h
