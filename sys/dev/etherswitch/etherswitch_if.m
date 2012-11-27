# $FreeBSD$

#include <sys/bus.h>

# Needed for ifreq/ifmediareq
#include <sys/socket.h>
#include <net/if.h>

#include <dev/etherswitch/etherswitch.h>

INTERFACE etherswitch;

#
# Return device info
#
METHOD etherswitch_info_t* getinfo {
	device_t	dev;
}

#
# Read switch register
#
METHOD int readreg {
	device_t	dev;
	int		reg;
};

#
# Write switch register
#
METHOD int writereg {
	device_t	dev;
	int		reg;
	int		value;
};

#
# Read PHY register
#
METHOD int readphyreg {
	device_t	dev;
	int		phy;
	int		reg;
};

#
# Write PHY register
#
METHOD int writephyreg {
	device_t	dev;
	int		phy;
	int		reg;
	int		value;
};

#
# Get port configuration
#
METHOD int getport {
	device_t	dev;
	etherswitch_port_t *vg;
}

#
# Set port configuration
#
METHOD int setport {
	device_t	dev;
	etherswitch_port_t *vg;
}

#
# Get VLAN group configuration
#
METHOD int getvgroup {
	device_t	dev;
	etherswitch_vlangroup_t *vg;
}

#
# Set VLAN group configuration
#
METHOD int setvgroup {
	device_t	dev;
	etherswitch_vlangroup_t *vg;
}
