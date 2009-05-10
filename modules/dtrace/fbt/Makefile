# $FreeBSD$

.PATH: ${.CURDIR}/../../../cddl/dev/fbt

KMOD=		fbt
SRCS=		fbt.c
SRCS+=		vnode_if.h

CFLAGS+=	-I${.CURDIR}/../../../cddl/compat/opensolaris \
		-I${.CURDIR}/../../../cddl/contrib/opensolaris/uts/common \
		-I${.CURDIR}/../../..

.include <bsd.kmod.mk>
