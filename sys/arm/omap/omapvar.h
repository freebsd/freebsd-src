/*
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Ben Gray.
 * 4. The name of the company nor the name of the author may be used to
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
 */

#ifndef _OMAPVAR_H_
#define	_OMAPVAR_H_

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

#include <arm/omap/omap_cpuid.h>


/*
 * Random collection of functions and definitions ... needs cleanup
 *
 * 
 *
 */


extern struct bus_space omap_bs_tag;

unsigned int
omap_sdram_size(void);

void
omap_mask_all_intr(void);

void
omap_post_filter_intr(void *arg);

int
omap_setup_intr(device_t dev, device_t child,
				 struct resource *res, int flags, driver_filter_t *filt, 
				 driver_intr_t *intr, void *arg, void **cookiep);

int
omap_teardown_intr(device_t dev, device_t child, struct resource *res,
					void *cookie);



/**
 *	OMAP Device IDs
 *
 *	These values are typically read out of the ID_CODE register, located at
 *	physical address 0x4A00 2204 on most OMAP devices
 */
#define OMAP_CPUID_OMAP3530   0x0C00
#define OMAP_CPUID_OMAP3525   0x4C00
#define OMAP_CPUID_OMAP3515   0x1C00
#define OMAP_CPUID_OMAP3503   0x5C00

#define OMAP_CPUID_OMAP4430_ES1_2   0xB852
#define OMAP_CPUID_OMAP4430         0xB95C




/**
 *	struct omap_softc
 *
 *	
 *
 */
extern uint32_t omap3_chip_id;

static inline int 
omap_cpu_is(uint32_t cpu)
{
	return ((omap3_chip_id & 0xffff) == cpu);
}


/**
 *	struct omap_softc
 *
 *	
 *
 */
struct omap_softc {
	device_t           sc_dev;
	bus_space_tag_t    sc_iotag;
	bus_space_handle_t sc_ioh;
	
	struct rman        sc_irq_rman;
	struct rman        sc_mem_rman;
	bus_dma_tag_t      sc_dmat;
};


struct omap_mem_range {
	bus_addr_t  base;
	bus_size_t  size;
};

/**
 *	struct omap_cpu_dev
 *
 *	Structure used to define all the SoC devices, it allows for two memory
 *	address ranges and 4 IRQ's per device.
 *
 */
struct omap_cpu_dev {
	const char *name;
	int         unit;
	
	struct omap_mem_range mem[16];
	int                   irqs[16];
};



struct omap_ivar {
	struct resource_list resources;
};





#endif /* _OMAP3VAR_H_ */
