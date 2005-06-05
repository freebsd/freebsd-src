/*
 * WPA Supplicant / SSL/TLS interface functions for no TLS case
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

void * tls_init(void)
{
	return (void *) 1;
}

void tls_deinit(void *ssl_ctx)
{
}
