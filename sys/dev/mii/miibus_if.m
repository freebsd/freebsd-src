# $FreeBSD: src/sys/dev/mii/miibus_if.m,v 1.4.32.1 2008/11/25 02:59:29 kensmith Exp $

#include <sys/bus.h>

INTERFACE miibus;

#
# Read register from device on MII bus
#
METHOD int readreg {
	device_t		dev;
	int			phy;
	int			reg;
};

#
# Write register to device on MII bus
#
METHOD int writereg {
	device_t		dev;
	int			phy;
	int			reg;
	int			val;
};

#
# Notify bus about PHY status change.
#
METHOD void statchg {
	device_t		dev;
};

#
# Notify bus about PHY link change.
#
METHOD void linkchg {
	device_t		dev;
};

#
# Notify bus that media has been set.
#
METHOD void mediainit {
	device_t		dev;
};
