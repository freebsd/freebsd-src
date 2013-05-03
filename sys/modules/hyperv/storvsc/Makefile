#$FreeBSD$

.PATH:  ${.CURDIR}/../../../dev/hyperv/storvsc

KMOD=	 hv_storvsc

SRCS =	hv_storvsc_drv_freebsd.c \
	hv_vstorage.h

CFLAGS+= -I${.CURDIR}/../../../dev/hyperv/include \
	 -I${.CURDIR}/../../../dev/hyperv/vmbus	\
	 -I${.CURDIR}/../../../dev/hyperv/storvsc

.include <bsd.kmod.mk>
