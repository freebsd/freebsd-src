# $FreeBSD$

S	= ${.CURDIR}/../..
.PATH:  ${.CURDIR}/../../dev/pcic
KMOD=   pcic
SRCS=   i82365.c i82365_isa.c i82365_isasubr.c \
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
