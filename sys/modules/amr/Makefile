# $FreeBSD$

S		= ${.CURDIR}/../..
.PATH:		$S/dev/amr
KMOD		= amr
SRCS		= amr.c amr_pci.c amr_disk.c device_if.h bus_if.h pci_if.h 
CLEANFILES	+= amr.h device_if.h bus_if.h pci_if.h
CFLAGS		+= ${DEBUG_FLAGS}

amr.h:
	echo "#define NAMR 1" > amr.h

device_if.h: $S/kern/makedevops.pl $S/kern/device_if.m
	perl $S/kern/makedevops.pl -h $S/kern/device_if.m

bus_if.h: $S/kern/makedevops.pl $S/kern/bus_if.m
	perl $S/kern/makedevops.pl -h $S/kern/bus_if.m

pci_if.h: $S/kern/makedevops.pl $S/pci/pci_if.m
	perl $S/kern/makedevops.pl -h $S/pci/pci_if.m

.include <bsd.kmod.mk>
