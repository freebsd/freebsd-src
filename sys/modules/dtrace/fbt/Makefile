# $FreeBSD$

SYSDIR?=	${.CURDIR}/../../..

.PATH:		${SYSDIR}/cddl/dev/fbt

KMOD=		fbt
.if ${MACHINE_CPUARCH} == "powerpc"
SRCS=		fbt_powerpc.c
.else
SRCS=		fbt.c
.endif
SRCS+=		vnode_if.h

CFLAGS+=	-I${SYSDIR}/cddl/compat/opensolaris \
		-I${SYSDIR}/cddl/contrib/opensolaris/uts/common \
		-I${SYSDIR}

.include <bsd.kmod.mk>

CFLAGS+=	-include ${SYSDIR}/cddl/compat/opensolaris/sys/debug_compat.h
