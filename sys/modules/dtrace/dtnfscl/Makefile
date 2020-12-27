# $FreeBSD$

SYSDIR?=	${SRCTOP}/sys

.PATH:		${SYSDIR}/fs/nfsclient

KMOD=		dtnfscl
SRCS=		nfs_clkdtrace.c \
		vnode_if.h

CFLAGS+=	${OPENZFS_CFLAGS}

.include <bsd.kmod.mk>

CFLAGS+=	-include ${SYSDIR}/cddl/compat/opensolaris/sys/debug_compat.h
CWARNFLAGS+=	${OPENZFS_CWARNFLAGS}
