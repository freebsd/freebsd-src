#	$Id: Makefile,v 1.3 1999/07/06 19:23:01 des Exp $

S	= ${.CURDIR}/../..
.PATH:	$S/pci
KMOD	= xl
SRCS	= if_xl.c xl.h bpf.h opt_bdg.h device_if.h bus_if.h pci_if.h
CLEANFILES	+= xl.h bpf.h opt_bdg.h device_if.h bus_if.h pci_if.h
CFLAGS	+= ${DEBUG_FLAGS}

xl.h:
	echo "#define NFXP 1" > xl.h

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
