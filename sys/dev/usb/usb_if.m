# USB interface description
#

INTERFACE usb;

# The device should start probing for new children again
#
METHOD int reconfigure {
	device_t dev;
};

