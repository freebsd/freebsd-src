SYSDIR?=	${SRCTOP}/sys

.PATH:		${SYSDIR}/cddl/dev/profile

KMOD=		profile
SRCS=		profile.c
SRCS+=		vnode_if.h

CFLAGS+= 	${OPENZFS_CFLAGS}

.include <bsd.kmod.mk>

CFLAGS+=	-include ${SYSDIR}/cddl/compat/opensolaris/sys/debug_compat.h
