#	$Id$

S	= ${.CURDIR}/../..
.PATH:	$S/pci
KMOD	= fxp
SRCS	= if_fxp.c fxp.h bpfilter.h opt_bdg.h device_if.h bus_if.h pci_if.h
CLEANFILES	+= fxp.h bpfilter.h opt_bdg.h device_if.h bus_if.h pci_if.h
CFLAGS	+= ${DEBUG_FLAGS}

fxp.h:
	echo "#define NFXP 1" > fxp.h

bpfilter.h:
	echo "#define NBPFILTER 0" > bpfilter.h

opt_bdg.h:
	touch opt_bdg.h

device_if.h: $S/kern/makedevops.pl $S/kern/device_if.m
	perl5 $S/kern/makedevops.pl -h $S/kern/device_if.m

bus_if.h: $S/kern/makedevops.pl $S/kern/bus_if.m
	perl5 $S/kern/makedevops.pl -h $S/kern/bus_if.m

pci_if.h: $S/kern/makedevops.pl $S/pci/pci_if.m
	perl5 $S/kern/makedevops.pl -h $S/pci/pci_if.m

.include <bsd.kmod.mk>
