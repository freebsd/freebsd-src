# $FreeBSD$

.PATH:	${SRCTOP}/sys/dev/qat_c2xxx

KMOD=	qat_c2xxx

SRCS=	qat.c		\
	qat_ae.c	\
	qat_c2xxx.c	\
	qat_hw15.c

SRCS+=	bus_if.h cryptodev_if.h device_if.h pci_if.h

.include <bsd.kmod.mk>
