# $FreeBSD$

S	= ${.CURDIR}/../..
.PATH:	$S/dev/mii
KMOD	= miibus
SRCS	= mii.c mii_physubr.c ukphy.c ukphy_subr.c bus_if.h
SRCS	+= miibus_if.h device_if.h miibus_if.c exphy.c nsphy.c
SRCS	+= mlphy.c tlphy.c rlphy.c amphy.c mxphy.c
CLEANFILES	+= device_if.h bus_if.h miibus_if.h miibus_if.c
CFLAGS	+= ${DEBUG_FLAGS}

device_if.h: $S/kern/makedevops.pl $S/kern/device_if.m
	perl $S/kern/makedevops.pl -h $S/kern/device_if.m

bus_if.h: $S/kern/makedevops.pl $S/kern/bus_if.m
	perl $S/kern/makedevops.pl -h $S/kern/bus_if.m

miibus_if.h: $S/kern/makedevops.pl $S/dev/mii/miibus_if.m
	perl $S/kern/makedevops.pl -h $S/dev/mii/miibus_if.m

miibus_if.c: $S/kern/makedevops.pl $S/dev/mii/miibus_if.m
	perl $S/kern/makedevops.pl -c $S/dev/mii/miibus_if.m

.include <bsd.kmod.mk>
