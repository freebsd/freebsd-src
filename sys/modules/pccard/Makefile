# $FreeBSD$

S	= ${.CURDIR}/../..
.PATH:  ${.CURDIR}/../../dev/pccard
KMOD=   pccard
SRCS=   pccard.c pccard_cis.c pccard_cis_quirks.c \
	device_if.h bus_if.h isa_if.h
CLEANFILES+= device_if.h bus_if.h isa_if.h
NOMAN=

device_if.h: $S/kern/makedevops.pl $S/kern/device_if.m
	perl $S/kern/makedevops.pl -h $S/kern/device_if.m

bus_if.h: $S/kern/makedevops.pl $S/kern/bus_if.m
	perl $S/kern/makedevops.pl -h $S/kern/bus_if.m

isa_if.h: $S/kern/makedevops.pl $S/isa/isa_if.m
	perl $S/kern/makedevops.pl -h $S/isa/isa_if.m

.include <bsd.kmod.mk>
