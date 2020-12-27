# $FreeBSD$

SYSDIR?=	${SRCTOP}/sys

.PATH:		${SYSDIR}/cddl/dev/fbt

KMOD=		fbt
SRCS=		fbt.c fbt_isa.c
SRCS+=		vnode_if.h


.if ${MACHINE_CPUARCH} == "amd64" || ${MACHINE_CPUARCH} == "i386"
CFLAGS+=	-I${SYSDIR}/cddl/dev/fbt/x86
.PATH:		${SYSDIR}/cddl/dev/fbt/x86
.else
CFLAGS+=	-I${SYSDIR}/cddl/dev/fbt/${MACHINE_CPUARCH}
.PATH:		${SYSDIR}/cddl/dev/fbt/${MACHINE_CPUARCH}
.endif

CFLAGS+= 	${OPENZFS_CFLAGS}
CFLAGS+=  -I${SYSDIR}/cddl/dev/fbt

.include <bsd.kmod.mk>

CFLAGS+=	-include ${SYSDIR}/cddl/compat/opensolaris/sys/debug_compat.h
CWARNFLAGS+=	${OPENZFS_CWARNFLAGS}
