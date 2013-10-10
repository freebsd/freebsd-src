# $FreeBSD$

.PATH: ${.CURDIR}/../../kern

KMOD=	mqueuefs
SRCS=	uipc_mqueue.c \
	vnode_if.h \
	opt_posix.h opt_compat.h opt_capsicum.h

.include <bsd.kmod.mk>
