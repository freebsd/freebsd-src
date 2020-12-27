# $FreeBSD$

SYSDIR?=	${SRCTOP}/sys

.PATH:		${SYSDIR}/cddl/dev/systrace

KMOD=		systrace
SRCS=		systrace.c
SRCS+=		vnode_if.h

CFLAGS+=	${OPENZFS_CFLAGS}

.include <bsd.kmod.mk>

CFLAGS+=	-include ${SYSDIR}/cddl/compat/opensolaris/sys/debug_compat.h
CWARNFLAGS+=	${OPENZFS_CWARNFLAGS}
