# $FreeBSD$

S	= ${.CURDIR}/../..
.PATH:	$S/pci
KMOD	= if_ti
SRCS	= if_ti.c opt_bdg.h vlan.h device_if.h bus_if.h pci_if.h
CLEANFILES	+= opt_bdg.h vlan.h device_if.h bus_if.h pci_if.h
CFLAGS	+= ${DEBUG_FLAGS}

opt_bdg.h:
	touch opt_bdg.h

vlan.h:
	touch vlan.h

device_if.h: $S/kern/makedevops.pl $S/kern/device_if.m
	perl $S/kern/makedevops.pl -h $S/kern/device_if.m

bus_if.h: $S/kern/makedevops.pl $S/kern/bus_if.m
	perl $S/kern/makedevops.pl -h $S/kern/bus_if.m

pci_if.h: $S/kern/makedevops.pl $S/pci/pci_if.m
	perl $S/kern/makedevops.pl -h $S/pci/pci_if.m

.include <bsd.kmod.mk>
