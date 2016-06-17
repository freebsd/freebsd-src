#ifndef _DN_ACCEL_H_
#define _DN_ACCEL_H_

#include <linux/fb.h>

void dn_bitblt(struct display *p,int x_src,int y_src, int x_dest, int y_dest,
	       int x_count, int y_count);

#endif
