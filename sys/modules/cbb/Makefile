# $FreeBSD$

S	= ${.CURDIR}/../..
.PATH:  ${.CURDIR}/../../pci
KMOD=   cbb
SRCS=   pccbb.c \
	device_if.h bus_if.h
CLEANFILES+= device_if.h bus_if.h
NOMAN=

device_if.h: $S/kern/makedevops.pl $S/kern/device_if.m
	perl $S/kern/makedevops.pl -h $S/kern/device_if.m

bus_if.h: $S/kern/makedevops.pl $S/kern/bus_if.m
	perl $S/kern/makedevops.pl -h $S/kern/bus_if.m

.include <bsd.kmod.mk>
