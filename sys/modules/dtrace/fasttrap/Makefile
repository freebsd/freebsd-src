# $FreeBSD$

.PATH: ${.CURDIR}/../../../cddl/dev/fasttrap

KMOD=		fasttrap
SRCS=		fasttrap.c
SRCS+=		vnode_if.h

CFLAGS+=	-I${.CURDIR}/../../../cddl/compat/opensolaris \
		-I${.CURDIR}/../../../cddl/contrib/opensolaris/uts/common \
		-I${.CURDIR}/../../..

.if ${MACHINE_CPUARCH} == "amd64" ||  ${MACHINE_CPUARCH} == "i386"
CFLAGS+=	-I${.CURDIR}/../../../cddl/contrib/opensolaris/uts/intel
.endif

CFLAGS+=	-DSMP -DDEBUG

.include <bsd.kmod.mk>
