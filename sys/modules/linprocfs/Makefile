# $FreeBSD: src/sys/modules/linprocfs/Makefile,v 1.14.12.1 2010/12/21 17:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../../compat/linprocfs

KMOD=	linprocfs
SRCS=	vnode_if.h \
	linprocfs.c \
	opt_compat.h

.if ${MACHINE_ARCH} == "amd64"
CFLAGS+=-DCOMPAT_LINUX32
.endif

.include <bsd.kmod.mk>
