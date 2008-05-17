# $FreeBSD$

.PATH: ${.CURDIR}/../../cddl/cyclic

KMOD=		cyclic
SRCS=		cyclic.c
		
SRCS+=		vnode_if.h

CFLAGS+=	-I${.CURDIR}/../../cddl/compat/opensolaris		\
		-I${.CURDIR}/../../cddl/contrib/opensolaris/uts/common	\
		-I${.CURDIR}/../..					\
		-I${.CURDIR}/../../cddl/cyclic/${MACHINE_ARCH}

CFLAGS+=	-DDEBUG=1

IGNORE_PRAGMA=	1

.include <bsd.kmod.mk>
