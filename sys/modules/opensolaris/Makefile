# $FreeBSD$

.PATH: ${.CURDIR}/../../cddl/compat/opensolaris/kern

KMOD=		opensolaris
SRCS=		opensolaris.c		\
		opensolaris_cmn_err.c	\
		opensolaris_kmem.c	\
		opensolaris_misc.c

.if ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "ia64" || ${MACHINE_ARCH} == "sparc64" || ${MACHINE_ARCH} == "powerpc64"
.PATH:	${.CURDIR}/../../cddl/contrib/opensolaris/common/atomic/${MACHINE_ARCH}
SRCS+=		opensolaris_atomic.S
.else
SRCS+=		opensolaris_atomic.c
.endif

CFLAGS+=	-I${.CURDIR}/../../cddl/compat/opensolaris		\
		-I${.CURDIR}/../../cddl/contrib/opensolaris/uts/common	\
		-I${.CURDIR}/../..

IGNORE_PRAGMA=	1

.include <bsd.kmod.mk>
