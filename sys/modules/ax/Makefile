# $FreeBSD$

S	= ${.CURDIR}/../..
.PATH:	$S/pci
KMOD	= if_ax
SRCS	= if_ax.c ax.h opt_bdg.h device_if.h bus_if.h pci_if.h
CLEANFILES	+= ax.h opt_bdg.h device_if.h bus_if.h pci_if.h
CFLAGS	+= ${DEBUG_FLAGS}

ax.h:
	echo "#define NAX 1" > ax.h

opt_bdg.h:
	touch opt_bdg.h

device_if.h: $S/kern/makedevops.pl $S/kern/device_if.m
	perl $S/kern/makedevops.pl -h $S/kern/device_if.m

bus_if.h: $S/kern/makedevops.pl $S/kern/bus_if.m
	perl $S/kern/makedevops.pl -h $S/kern/bus_if.m

pci_if.h: $S/kern/makedevops.pl $S/pci/pci_if.m
	perl $S/kern/makedevops.pl -h $S/pci/pci_if.m

.include <bsd.kmod.mk>
