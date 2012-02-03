/*-
 * Copyright (c) 2005 Olivier Houchard.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* $FreeBSD$ */

#ifndef _AT91VAR_H_
#define _AT91VAR_H_

#include <sys/bus.h>
#include <sys/rman.h>

#include <arm/at91/at91reg.h>

struct at91_softc {
	device_t dev;
	bus_space_tag_t sc_st;
	bus_space_handle_t sc_sh;
	bus_space_handle_t sc_aic_sh;
	struct rman sc_irq_rman;
	struct rman sc_mem_rman;
	uint32_t sc_irq_system;
};

struct at91_ivar {
	struct resource_list resources;
};

struct cpu_devs
{
	const char *name;
	int unit;
	bus_addr_t mem_base;
	bus_size_t mem_len;
	int irq0;
	int irq1;
	int irq2;
	const char *parent_clk;
};

extern uint32_t at91_chip_id;

static inline int at91_is_rm92(void);
static inline int at91_is_sam9(void) ;
static inline int at91_cpu_is(u_int cpu);

static inline int 
at91_is_rm92(void) 
{
	return (AT91_ARCH(at91_chip_id) == AT91_ARCH_RM92);
}

static inline int 
at91_is_sam9(void) 
{
	return (AT91_ARCH(at91_chip_id) == AT91_ARCH_SAM9);
}

static inline int 
at91_cpu_is(u_int cpu)
{
	return (AT91_CPU(at91_chip_id) == cpu);
}

extern uint32_t at91_irq_system;
extern uint32_t at91_master_clock;

#endif /* _AT91VAR_H_ */
