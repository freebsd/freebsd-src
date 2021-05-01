# $FreeBSD$

.PATH:	${SRCTOP}/sys/dev/qat

KMOD=	qat

SRCS=	qat.c		\
	qat_ae.c	\
	qat_c2xxx.c	\
	qat_c3xxx.c	\
	qat_c62x.c	\
	qat_d15xx.c	\
	qat_dh895xcc.c	\
	qat_hw15.c	\
	qat_hw17.c

SRCS+=	bus_if.h cryptodev_if.h device_if.h pci_if.h

.include <bsd.kmod.mk>
