# $FreeBSD$

.PATH:  ${.CURDIR}/../../../contrib/dev/hyperv/storvsc

KMOD=	 hv_storvsc

SRCS =	hv_storvsc_drv_freebsd.c \
	hv_vstorage.h

CFLAGS+= -I${.CURDIR}/../../../contrib/dev/hyperv/include \
	 -I${.CURDIR}/../../../contrib/dev/hyperv/vmbus	\
	 -I${.CURDIR}/../../../contrib/dev/hyperv/storvsc \
	 -I${.CURDIR}/../../../contrib

.include <bsd.kmod.mk>
