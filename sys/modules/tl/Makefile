#	$Id: Makefile,v 1.3 1999/08/27 19:45:18 wpaul Exp $

S	= ${.CURDIR}/../..
.PATH:	$S/pci
KMOD	= tl
SRCS	= if_tl.c tl.h bpf.h opt_bdg.h device_if.h bus_if.h pci_if.h
SRCS	+= miibus_if.h
CLEANFILES	+= tl.h bpf.h opt_bdg.h device_if.h bus_if.h pci_if.h
CLEANFILEs	+= miibus_if.h
CFLAGS	+= ${DEBUG_FLAGS}

tl.h:
	echo "#define NTL 1" > tl.h

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

miibus_if.h: $S/kern/makedevops.pl $S/dev/mii/miibus_if.m
	perl $S/kern/makedevops.pl -h $S/dev/mii/miibus_if.m

.include <bsd.kmod.mk>
