#	$Id: Makefile,v 1.1 1999/07/25 04:32:38 wpaul Exp $

S	= ${.CURDIR}/../..
.PATH:	$S/pci
KMOD	= sf
SRCS	= if_sf.c sf.h bpf.h opt_bdg.h device_if.h bus_if.h pci_if.h
CLEANFILES	+= sf.h bpf.h opt_bdg.h device_if.h bus_if.h pci_if.h
CFLAGS	+= ${DEBUG_FLAGS}

sf.h:
	echo "#define NSF 1" > sf.h

bpf.h:
	echo "#define NBPF 1" > bpf.h

opt_bdg.h:
	touch opt_bdg.h

device_if.h: $S/kern/makedevops.pl $S/kern/device_if.m
	perl $S/kern/makedevops.pl -h $S/kern/device_if.m

bus_if.h: $S/kern/makedevops.pl $S/kern/bus_if.m
	perl $S/kern/makedevops.pl -h $S/kern/bus_if.m

pci_if.h: $S/kern/makedevops.pl $S/pci/pci_if.m
	perl $S/kern/makedevops.pl -h $S/pci/pci_if.m

.include <bsd.kmod.mk>
