# $FreeBSD$

.PATH:		${.CURDIR}/../../dev/mlx
KMOD		= mlx
SRCS		= mlx.c mlx_pci.c mlx_disk.c device_if.h bus_if.h pci_if.h
CLEANFILES	= mlx.h
CFLAGS		+= ${DEBUG_FLAGS}

mlx.h:
	echo "#define NMLX 1" > mlx.h

.include <bsd.kmod.mk>
