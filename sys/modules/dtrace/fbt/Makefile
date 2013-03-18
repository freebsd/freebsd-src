# $FreeBSD$

.PATH: ${.CURDIR}/../../../cddl/dev/fbt

KMOD=		fbt
.if ${MACHINE_ARCH} == "powerpc"
SRCS=		fbt_powerpc.c
.else
SRCS=		fbt.c
.endif
SRCS+=		vnode_if.h

CFLAGS+=	-I${.CURDIR}/../../../cddl/compat/opensolaris \
		-I${.CURDIR}/../../../cddl/contrib/opensolaris/uts/common \
		-I${.CURDIR}/../../..

.include <bsd.kmod.mk>
