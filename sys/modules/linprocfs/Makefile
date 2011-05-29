# $FreeBSD$

.PATH: ${.CURDIR}/../../compat/linprocfs

KMOD=	linprocfs
SRCS=	vnode_if.h \
	device_if.h bus_if.h \
	linprocfs.c \
	opt_compat.h

.if ${MACHINE_CPUARCH} == "amd64"
CFLAGS+=-DCOMPAT_LINUX32
.endif

.include <bsd.kmod.mk>
