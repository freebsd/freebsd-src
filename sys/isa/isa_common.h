/*-
 * Copyright (c) 1999 Doug Rabson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/isa/isa_common.h,v 1.5 1999/09/07 08:42:47 dfr Exp $
 */

/*
 * Parts of the ISA bus implementation common to all architectures.
 *
 * Drivers must not depend on information in this file as it can change
 * without notice.
 */

MALLOC_DECLARE(M_ISADEV);

/*
 * PNP configurations are kept in a tailq.
 */
TAILQ_HEAD(isa_config_list, isa_config_entry);
struct isa_config_entry {
	TAILQ_ENTRY(isa_config_entry) ice_link;
	int			ice_priority;
	struct isa_config	ice_config;
};

/*
 * The structure used to attach devices to the isa bus.
 */
struct isa_device {
	struct resource_list	id_resources;
	u_int32_t		id_vendorid; /* pnp vendor id */
	u_int32_t		id_serial; /* pnp serial */
	u_int32_t		id_logicalid; /* pnp logical device id */
	u_int32_t		id_compatid; /* pnp compat device id */
	struct isa_config_list	id_configs; /* pnp config alternatives */
	isa_config_cb		*id_config_cb; /* callback function */
	void			*id_config_arg;	/* callback argument */
};

#define DEVTOISA(dev)	((struct isa_device *) device_get_ivars(dev))

/*
 * These functions are architecture dependant.
 */
extern void isa_init(void);
extern struct resource *isa_alloc_resource(device_t bus, device_t child,
					   int type, int *rid,
					   u_long start, u_long end,
					   u_long count, u_int flags);
extern int isa_release_resource(device_t bus, device_t child,
				int type, int rid,
				struct resource *r);

/* XXX alphe declares these elsewhere */
#ifndef __alpha__
extern int isa_setup_intr(device_t bus, device_t child,
			  struct resource *r, int flags,
			  void (*ihand)(void *), void *arg, void **cookiep);
extern int isa_teardown_intr(device_t bus, device_t child,
			     struct resource *r, void *cookie);
#endif

