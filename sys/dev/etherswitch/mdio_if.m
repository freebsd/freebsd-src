# $FreeBSD$

#include <sys/bus.h>

INTERFACE mdio;

#
# Read register from device on MDIO bus
#
METHOD int readreg {
	device_t		dev;
	int			phy;
	int			reg;
};

#
# Write register to device on MDIO bus
#
METHOD int writereg {
	device_t		dev;
	int			phy;
	int			reg;
	int			val;
};
