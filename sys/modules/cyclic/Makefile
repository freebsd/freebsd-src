# $FreeBSD: src/sys/modules/cyclic/Makefile,v 1.2.4.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../cddl/dev/cyclic

KMOD=		cyclic
SRCS=		cyclic.c
		
SRCS+=		vnode_if.h

CFLAGS+=	-I${.CURDIR}/../../cddl/compat/opensolaris		\
		-I${.CURDIR}/../../cddl/contrib/opensolaris/uts/common	\
		-I${.CURDIR}/../..					\
		-I${.CURDIR}/../../cddl/dev/cyclic/${MACHINE_ARCH}

CFLAGS+=	-DDEBUG=1

IGNORE_PRAGMA=	1

.include <bsd.kmod.mk>
