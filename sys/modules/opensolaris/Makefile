# $FreeBSD: src/sys/modules/opensolaris/Makefile,v 1.1.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../cddl/compat/opensolaris/kern

KMOD=		opensolaris
SRCS=		opensolaris.c		\
		opensolaris_cmn_err.c	\
		opensolaris_kmem.c

.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "ia64"
.PATH:	${.CURDIR}/../../cddl/contrib/opensolaris/common/atomic/${MACHINE_ARCH}
SRCS+=		atomic.S
.else
SRCS+=		opensolaris_atomic.c
.endif
		
CFLAGS+=	-I${.CURDIR}/../../cddl/compat/opensolaris		\
		-I${.CURDIR}/../../cddl/contrib/opensolaris/uts/common	\
		-I${.CURDIR}/../..

IGNORE_PRAGMA=	1

.include <bsd.kmod.mk>
