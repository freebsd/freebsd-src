# $FreeBSD$

S	= ${.CURDIR}/../..
.PATH:	$S/pci
KMOD	= if_wb
SRCS	= if_wb.c wb.h opt_bdg.h device_if.h
SRCS	+= bus_if.h miibus_if.h pci_if.h
CLEANFILES	+= wb.h opt_bdg.h device_if.h
CLEANFILES	+= bus_if.h miibus_if.h pci_if.h
CFLAGS	+= ${DEBUG_FLAGS}
KMODDEPS = miibus

wb.h:
	echo "#define NWB 1" > wb.h

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
