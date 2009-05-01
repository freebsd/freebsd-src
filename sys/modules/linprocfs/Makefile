# $FreeBSD: src/sys/modules/linprocfs/Makefile,v 1.14.8.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../compat/linprocfs

KMOD=	linprocfs
SRCS=	vnode_if.h \
	linprocfs.c \
	opt_compat.h

.if ${MACHINE_ARCH} == "amd64"
CFLAGS+=-DCOMPAT_LINUX32
.endif

.include <bsd.kmod.mk>
