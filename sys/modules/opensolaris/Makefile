# $FreeBSD$

.PATH: ${.CURDIR}/../../cddl/compat/opensolaris/kern

KMOD=		opensolaris
SRCS=		opensolaris.c		\
		opensolaris_cmn_err.c	\
		opensolaris_kmem.c	\
		opensolaris_misc.c

.if ${MACHINE_CPUARCH} == "i386" || ${MACHINE_CPUARCH} == "amd64" || ${MACHINE_CPUARCH} == "ia64" || ${MACHINE_CPUARCH} == "sparc64"
.PATH:	${.CURDIR}/../../cddl/contrib/opensolaris/common/atomic/${MACHINE_CPUARCH}
SRCS+=		opensolaris_atomic.S
.else
SRCS+=		opensolaris_atomic.c
.endif

CFLAGS+=	-I${.CURDIR}/../../cddl/compat/opensolaris		\
		-I${.CURDIR}/../../cddl/contrib/opensolaris/uts/common	\
		-I${.CURDIR}/../..

IGNORE_PRAGMA=	1

.include <bsd.kmod.mk>
