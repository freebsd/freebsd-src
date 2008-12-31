# $FreeBSD: src/sys/modules/linprocfs/Makefile,v 1.14.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../compat/linprocfs

KMOD=	linprocfs
SRCS=	vnode_if.h \
	linprocfs.c \
	opt_compat.h

.if ${MACHINE_ARCH} == "amd64"
CFLAGS+=-DCOMPAT_LINUX32
.endif

.include <bsd.kmod.mk>
