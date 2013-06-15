# $FreeBSD$

.PATH:	${.CURDIR}/../../../contrib/dev/hyperv/utilities

KMOD=	 hv_utils

SRCS =  hv_util.c

CFLAGS+= -I${.CURDIR}/../../../contrib/dev/hyperv/include \
	 -I${.CURDIR}/../../../contrib/dev/hyperv/vmbus \
	 -I${.CURDIR}/../../../contrib

.include <bsd.kmod.mk>
