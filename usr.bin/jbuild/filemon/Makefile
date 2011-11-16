# $FreeBSD$

MAINTAINER=	obrien@FreeBSD.org

KMOD=	filemon
SRCS=	${KMOD}.c
SRCS+=	vnode_if.h opt_compat.h opt_capsicum.h

.if ${MACHINE_ARCH} == "amd64"
CFLAGS=	-DCOMPAT_IA32
.endif

SUBDIR=	test

.include <bsd.kmod.mk>
