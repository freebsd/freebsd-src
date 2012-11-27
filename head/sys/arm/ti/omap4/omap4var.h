/*-
 * Copyright (c) 2010
 *	Ben Gray <ben.r.gray@gmail.com>.
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
 * 3. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BEN GRAY ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BEN GRAY BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _OMAP4VAR_H_
#define	_OMAP4VAR_H_

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/time.h>
#include <sys/bus.h>
#include <sys/resource.h>
#include <sys/rman.h>
#include <sys/sysctl.h>
#include <sys/endian.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/resource.h>
#include <machine/intr.h>


void omap4_mask_all_intr(void);
void omap4_post_filter_intr(void *arg);

struct omap4_softc {
	device_t sc_dev;
	bus_space_tag_t sc_iotag;
    
	/* Handles for the two generic interrupt controller (GIC) register mappings */
	bus_space_handle_t sc_gic_cpu_ioh;
	bus_space_handle_t sc_gic_dist_ioh;
	
	/* Handle for the PL310 L2 cache controller */
	bus_space_handle_t sc_pl310_ioh;
	
	/* Handle for the global and provate timer register set in the Cortex core */
	bus_space_handle_t sc_prv_timer_ioh;
	bus_space_handle_t sc_gbl_timer_ioh;
	
	/* SCM access */
	struct resource *sc_scm_mem;
	int sc_scm_rid;
};

struct omap4_intr_conf {
	int            num;
	unsigned int   priority;
	unsigned int   target_cpu;
};

int omap4_setup_intr_controller(device_t dev,
    const struct omap4_intr_conf *irqs);
int omap4_setup_gic_cpu(unsigned int prio_mask);

void omap4_init_timer(device_t dev);

int omap4_setup_l2cache_controller(struct omap4_softc *sc);
void omap4_smc_call(uint32_t fn, uint32_t arg);

#endif /* _OMAP4VAR_H_ */
