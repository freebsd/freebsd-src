# $FreeBSD$

SYSDIR?=	${SRCTOP}/sys

.PATH:		${SYSDIR}/cddl/compat/opensolaris/kern
.PATH:		${SYSDIR}/contrib/openzfs/module/os/freebsd/spl

KMOD=		opensolaris
SRCS=		vnode_if.h \
		opensolaris.c		\
		opensolaris_proc.c	\
		spl_cmn_err.c	\
		spl_kmem.c	\
		spl_misc.c	\
		spl_sunddi.c

_A=${SYSDIR}/cddl/contrib/opensolaris/common/atomic
.if exists(${_A}/${MACHINE_CPUARCH}/opensolaris_atomic.S)
.PATH:	${_A}/${MACHINE_CPUARCH}
SRCS+=		opensolaris_atomic.S
.elif exists(${_A}/${MACHINE_ARCH}/opensolaris_atomic.S)
.PATH:	${_A}/${MACHINE_ARCH}
SRCS+=		opensolaris_atomic.S
.else
SRCS+=		opensolaris_atomic.c
.endif

CFLAGS+= 	${OPENZFS_CFLAGS}

EXPORT_SYMS=	cpu_core

IGNORE_PRAGMA=	1

.include <bsd.kmod.mk>

CFLAGS+=	-include ${SYSDIR}/cddl/compat/opensolaris/sys/debug_compat.h
