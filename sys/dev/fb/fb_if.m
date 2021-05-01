#include <sys/bus.h>
#include <sys/fbio.h>

INTERFACE fb;

METHOD struct fb_info * getinfo {
	device_t dev;
};
