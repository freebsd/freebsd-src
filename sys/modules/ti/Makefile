#	$Id: Makefile,v 1.1 1999/07/23 05:47:58 wpaul Exp $

S	= ${.CURDIR}/../..
.PATH:	$S/pci
KMOD	= ti
SRCS	= if_ti.c ti.h bpf.h opt_bdg.h vlan.h device_if.h bus_if.h pci_if.h
CLEANFILES	+= ti.h bpf.h opt_bdg.h vlan.h device_if.h bus_if.h pci_if.h
CFLAGS	+= ${DEBUG_FLAGS}

ti.h:
	echo "#define NTI 1" > ti.h

bpf.h:
	echo "#define NBPF 1" > bpf.h

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
