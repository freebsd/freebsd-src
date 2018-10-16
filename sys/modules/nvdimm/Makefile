# $FreeBSD$

.PATH: ${SRCTOP}/sys/dev/nvdimm

KMOD=	nvdimm
SRCS=	nvdimm.c \
	nvdimm_spa.c

SRCS+=	device_if.h bus_if.h

.include <bsd.kmod.mk>
