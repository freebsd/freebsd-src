# $FreeBSD$

SYSDIR?=	${.CURDIR}/../..

.PATH:		${SYSDIR}/cddl/dev/cyclic

KMOD=		cyclic
SRCS=		cyclic.c

SRCS+=		vnode_if.h

CFLAGS+=	-I${SYSDIR}/cddl/compat/opensolaris		\
		-I${SYSDIR}/cddl/contrib/opensolaris/uts/common	\
		-I${SYSDIR}					\
		-I${SYSDIR}/cddl/dev/cyclic/i386

IGNORE_PRAGMA=	1

.include <bsd.kmod.mk>

CFLAGS+=	-include ${SYSDIR}/cddl/compat/opensolaris/sys/debug_compat.h
