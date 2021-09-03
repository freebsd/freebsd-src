# $FreeBSD$

.PATH: ${SRCTOP}/sys/dev/vmd

KMOD=	vmd

SRCS=	\
	vmd.c		\
	bus_if.h	\
	device_if.h	\
	pci_if.h 	\
	pcib_if.h	\
	opt_global.h	\

CFLAGS+=        -include opt_global.h

opt_global.h:
	echo "#define NEW_PCIB 1"	>> ${.TARGET}

.include <bsd.kmod.mk>
