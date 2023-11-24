
SYSDIR?=	${SRCTOP}/sys

.PATH:		${SYSDIR}/security/audit

KMOD=		dtaudit
SRCS=		audit_dtrace.c	\
		vnode_if.h

CFLAGS+= ${OPENZFS_CFLAGS}

.include <bsd.kmod.mk>

CFLAGS+=	-include ${SYSDIR}/cddl/compat/opensolaris/sys/debug_compat.h
