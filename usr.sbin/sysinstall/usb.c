/*
 * USB support for sysinstall
 *
 * $FreeBSD$
 *
 * Copyright (c) 2000 John Baldwin <jhb@FreeBSD.org>.  All rights reserved.
 *
 * This software may be used, modified, copied, and distributed, in
 * both source and binary form provided that the above copyright and
 * these terms are retained. Under no circumstances is the author
 * responsible for the proper functioning of this software, nor does
 * the author assume any responsibility for damages incurred with its
 * use.
 */

#include "sysinstall.h"
#include <sys/fcntl.h>
#include <sys/time.h>

void
usbInitialize(void)
{
    int fd;
    WINDOW *w;
    
    if (!RunningAsInit && !Fake) {
	/* It's not my job... */
	return;
    }

    if ((fd = open("/dev/usb", O_RDONLY)) < 0) {
	msgDebug("Can't open USB controller.\n");
	return;
    }
    close(fd);

    w = savescr();
    msgNotify("Initializing USB controller....");
    
    variable_set2("usbd_enable", "YES", 1);

    vsystem("/stand/usbd");
    restorescr(w);
}
