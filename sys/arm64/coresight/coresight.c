/*-
 * Copyright (c) 2018-2020 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <machine/bus.h>

#include <arm64/coresight/coresight.h>

static struct mtx cs_mtx;
struct coresight_device_list cs_devs;

int
coresight_register(struct coresight_desc *desc)
{
	struct coresight_device *cs_dev;

	cs_dev = malloc(sizeof(struct coresight_device),
	    M_CORESIGHT, M_WAITOK | M_ZERO);
	cs_dev->dev = desc->dev;
	cs_dev->pdata = desc->pdata;
	cs_dev->dev_type = desc->dev_type;

	mtx_lock(&cs_mtx);
	TAILQ_INSERT_TAIL(&cs_devs, cs_dev, link);
	mtx_unlock(&cs_mtx);

	return (0);
}

struct endpoint *
coresight_get_output_endpoint(struct coresight_platform_data *pdata)
{
	struct endpoint *endp;

	if (pdata->out_ports != 1)
		return (NULL);

	TAILQ_FOREACH(endp, &pdata->endpoints, link) {
		if (endp->input == 0)
			return (endp);
	}

	return (NULL);
}

struct coresight_device *
coresight_get_output_device(struct endpoint *endp, struct endpoint **out_endp)
{
	struct coresight_platform_data *pdata;
	struct coresight_device *cs_dev;
	struct endpoint *endp2;

	TAILQ_FOREACH(cs_dev, &cs_devs, link) {
		pdata = cs_dev->pdata;
		TAILQ_FOREACH(endp2, &cs_dev->pdata->endpoints, link) {
			switch (pdata->bus_type) {
			case CORESIGHT_BUS_FDT:
#ifdef FDT
				if (endp->their_node == endp2->my_node) {
					*out_endp = endp2;
					return (cs_dev);
				}
#endif
				break;

			case CORESIGHT_BUS_ACPI:
#ifdef DEV_ACPI
				if (endp->their_handle == endp2->my_handle) {
					*out_endp = endp2;
					return (cs_dev);
				}
#endif
				break;
			}
		}
	}

	return (NULL);
}

static void
coresight_init(void)
{

	mtx_init(&cs_mtx, "ARM Coresight", NULL, MTX_DEF);
	TAILQ_INIT(&cs_devs);
}

SYSINIT(coresight, SI_SUB_DRIVERS, SI_ORDER_FIRST, coresight_init, NULL);
