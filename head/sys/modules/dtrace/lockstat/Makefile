# $FreeBSD$

.PATH: ${.CURDIR}/../../../cddl/dev/lockstat

KMOD=		lockstat
SRCS=		lockstat.c 

SRCS+=		vnode_if.h opt_kdtrace.h

CFLAGS+=	-I${.CURDIR}/../../../cddl/compat/opensolaris \
		-I${.CURDIR}/../../../cddl/contrib/opensolaris/uts/common \
		-I${.CURDIR}/../../..

.include <bsd.kmod.mk>
