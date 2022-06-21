#include <sys/bus.h>
#include <sys/fbio.h>

INTERFACE fb;

CODE {
	static struct fb_info *
	fb_default_getinfo(device_t dev)
	{
		return (NULL);
	}
};

METHOD struct fb_info * getinfo {
	device_t dev;
} DEFAULT fb_default_getinfo;
