# $FreeBSD$

SYSDIR?=	${.CURDIR}/../../..

.PATH:		${SYSDIR}/cddl/dev/systrace

.if ${MACHINE} == "amd64"
KMOD=		systrace_linux32
.else
KMOD=		systrace_linux
.endif

SRCS=		systrace.c
SRCS+=		vnode_if.h

CFLAGS+=	-I${SYSDIR}/cddl/compat/opensolaris \
		-I${SYSDIR}/cddl/contrib/opensolaris/uts/common \
		-I${SYSDIR} -DLINUX_SYSTRACE

.include <bsd.kmod.mk>

CFLAGS+=	-include ${SYSDIR}/cddl/compat/opensolaris/sys/debug_compat.h
