# $FreeBSD$

.PATH: ${.CURDIR}/../../compat/linprocfs

KMOD=	linprocfs
SRCS=	vnode_if.h \
	linprocfs.c \
	opt_compat.h \
	opt_route.h

.if ${MACHINE_ARCH} == "amd64"
CFLAGS+=-DCOMPAT_LINUX32
.endif

.include <bsd.kmod.mk>
