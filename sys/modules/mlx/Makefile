# $FreeBSD$

S		= ${.CURDIR}/../..
.PATH:		$S/dev/mlx
KMOD		= mlx
SRCS		= mlx.c mlx_pci.c mlx_disk.c device_if.h bus_if.h pci_if.h
CLEANFILES	+= mlx.h device_if.h bus_if.h pci_if.h
CFLAGS		+= ${DEBUG_FLAGS}

mlx.h:
	echo "#define NMLX 1" > mlx.h

device_if.h: $S/kern/makedevops.pl $S/kern/device_if.m
	perl $S/kern/makedevops.pl -h $S/kern/device_if.m

bus_if.h: $S/kern/makedevops.pl $S/kern/bus_if.m
	perl $S/kern/makedevops.pl -h $S/kern/bus_if.m

pci_if.h: $S/kern/makedevops.pl $S/pci/pci_if.m
	perl $S/kern/makedevops.pl -h $S/pci/pci_if.m

.include <bsd.kmod.mk>
