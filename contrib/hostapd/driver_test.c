/*
 * Host AP (software wireless LAN access point) user space daemon for
 * Host AP kernel driver / Driver interface for development testing
 * Copyright (c) 2004, Jouni Malinen <jkmaline@cc.hut.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "hostapd.h"
#include "driver.h"


struct test_driver_data {
	struct driver_ops ops;
	struct hostapd_data *hapd;
};

static const struct driver_ops test_driver_ops;


static int test_driver_init(struct hostapd_data *hapd)
{
	struct test_driver_data *drv;

	drv = malloc(sizeof(struct test_driver_data));
	if (drv == NULL) {
		printf("Could not allocate memory for test driver data\n");
		return -1;
	}

	memset(drv, 0, sizeof(*drv));
	drv->ops = test_driver_ops;
	drv->hapd = hapd;

	hapd->driver = &drv->ops;
	return 0;
}


static void test_driver_deinit(void *priv)
{
	struct test_driver_data *drv = priv;

	drv->hapd->driver = NULL;

	free(drv);
}


static const struct driver_ops test_driver_ops = {
	.name = "test",
	.init = test_driver_init,
	.deinit = test_driver_deinit,
};


void test_driver_register(void)
{
	driver_register(test_driver_ops.name, &test_driver_ops);
}
