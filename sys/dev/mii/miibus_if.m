# $FreeBSD$

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
# Read software configuration data from device on MII bus.
#
METHOD uint64_t readvar {
	device_t		dev;
	int			var;
};

#
# Notify bus that media has been set.
#
METHOD void mediainit {
	device_t		dev;
};
