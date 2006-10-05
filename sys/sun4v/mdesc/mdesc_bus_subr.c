/*-
 * Copyright (c) 2006 Kip Macy
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/errno.h>
#include <sys/malloc.h>

#include <machine/mdesc_bus_subr.h>
#include <machine/cddl/mdesc.h>
#include <machine/cddl/mdesc_impl.h>

#include "mdesc_bus_if.h"

MALLOC_DEFINE(M_MDPROP, "mdesc", "machine description");

int
mdesc_bus_gen_setup_devinfo(struct mdesc_bus_devinfo *mbd, mde_cookie_t node)
{
	md_t *mdp;
	
        if (mbd == NULL)
                return (ENOMEM);
	
	mdp = md_get();
	
        /* The 'name' property is considered mandatory. */
        if ((md_get_prop_alloc(mdp, node, "name", MDET_PROP_STR, (uint8_t **)&mbd->mbd_name)) == -1)
                return (EINVAL);
        md_get_prop_alloc(mdp, node, "compatible", MDET_PROP_DAT, (uint8_t **)&mbd->mbd_compat);
        md_get_prop_alloc(mdp, node, "device-type", MDET_PROP_STR, (uint8_t **)&mbd->mbd_type);
        md_get_prop_val(mdp, node, "cfg-handle", &mbd->mbd_handle);

	md_put(mdp);
        return (0);
}

void
mdesc_bus_gen_destroy_devinfo(struct mdesc_bus_devinfo *mbd)
{

        if (mbd == NULL)
                return;
        if (mbd->mbd_compat != NULL)
                free(mbd->mbd_compat, M_MDPROP);
        if (mbd->mbd_name != NULL)
                free(mbd->mbd_name, M_MDPROP);
        if (mbd->mbd_type != NULL)
                free(mbd->mbd_type, M_MDPROP);
}

const char *
mdesc_bus_gen_get_compat(device_t bus, device_t dev)
{
        const struct mdesc_bus_devinfo *mbd;

        mbd = MDESC_BUS_GET_DEVINFO(bus, dev);
        if (mbd == NULL)
                return (NULL);
        return (mbd->mbd_compat);
}

const char *
mdesc_bus_gen_get_name(device_t bus, device_t dev)
{
        const struct mdesc_bus_devinfo *mbd;

        mbd = MDESC_BUS_GET_DEVINFO(bus, dev);
        if (mbd == NULL)
                return (NULL);
        return (mbd->mbd_name);
}

const char *
mdesc_bus_gen_get_type(device_t bus, device_t dev)
{
        const struct mdesc_bus_devinfo *mbd;

        mbd = MDESC_BUS_GET_DEVINFO(bus, dev);
        if (mbd == NULL)
                return (NULL);
        return (mbd->mbd_type);
}

uint64_t
mdesc_bus_gen_get_handle(device_t bus, device_t dev)
{
        const struct mdesc_bus_devinfo *mbd;

        mbd = MDESC_BUS_GET_DEVINFO(bus, dev);
        if (mbd == NULL)
                return (0);
        return (mbd->mbd_handle);
}









