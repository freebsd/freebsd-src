# $FreeBSD$

.PATH:		${.CURDIR}/../../dev/amr
KMOD		= amr
SRCS		= amr.c amr_pci.c amr_disk.c device_if.h bus_if.h pci_if.h 
CLEANFILES	= amr.h
CFLAGS		+= ${DEBUG_FLAGS}

amr.h:
	echo "#define NAMR 1" > amr.h

.include <bsd.kmod.mk>
