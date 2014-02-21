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

enum at91_soc_type {
	AT91_T_NONE = 0,
	AT91_T_CAP9,
	AT91_T_RM9200,
	AT91_T_SAM9260,
	AT91_T_SAM9261,
	AT91_T_SAM9263,
	AT91_T_SAM9G10,
	AT91_T_SAM9G20,
	AT91_T_SAM9G45,
	AT91_T_SAM9N12,
	AT91_T_SAM9RL,
	AT91_T_SAM9X5,
};

enum at91_soc_subtype {
	AT91_ST_ANY = -1,	/* Match any type */
	AT91_ST_NONE = 0,
	/* AT91RM9200 */
	AT91_ST_RM9200_BGA,
	AT91_ST_RM9200_PQFP,
	/* AT91SAM9260 */
	AT91_ST_SAM9XE,
	/* AT91SAM9G45 */
	AT91_ST_SAM9G45,
	AT91_ST_SAM9M10,
	AT91_ST_SAM9G46,
	AT91_ST_SAM9M11,
	/* AT91SAM9X5 */
	AT91_ST_SAM9G15,
	AT91_ST_SAM9G25,
	AT91_ST_SAM9G35,
	AT91_ST_SAM9X25,
	AT91_ST_SAM9X35,
};

enum at91_soc_family {
	AT91_FAMILY_SAM9 = 0x19,
	AT91_FAMILY_SAM9XE = 0x29,
	AT91_FAMILY_RM92 = 0x92,
};

#define AT91_SOC_NAME_MAX 50

typedef void (*DELAY_t)(int);
typedef void (*cpu_reset_t)(void);
typedef void (*clk_init_t)(void);

struct at91_soc_data {
	DELAY_t		soc_delay;		/* SoC specific delay function */
	cpu_reset_t	soc_reset;		/* SoC specific reset function */
	clk_init_t      soc_clock_init;		/* SoC specific clock init function */
	const int	*soc_irq_prio;		/* SoC specific IRQ priorities */
	const struct cpu_devs *soc_children;	/* SoC specific children list */
	const uint32_t  *soc_pio_base;		/* SoC specific PIO base registers */
	size_t          soc_pio_count;		/* Count of PIO units (not pins) in SoC */
};

struct at91_soc_info {
	enum at91_soc_type type;
	enum at91_soc_subtype subtype;
	enum at91_soc_family family;
	uint32_t cidr;
	uint32_t exid;
	char name[AT91_SOC_NAME_MAX];
	uint32_t dbgu_base;
	struct at91_soc_data *soc_data;
};

extern struct at91_soc_info soc_info;

static inline int at91_is_rm92(void);
static inline int at91_is_sam9(void);
static inline int at91_is_sam9xe(void);
static inline int at91_cpu_is(u_int cpu);

static inline int
at91_is_rm92(void)
{

	return (soc_info.type == AT91_T_RM9200);
}

static inline int
at91_is_sam9(void)
{

	return (soc_info.family == AT91_FAMILY_SAM9);
}

static inline int
at91_is_sam9xe(void)
{

	return (soc_info.family == AT91_FAMILY_SAM9XE);
}

static inline int
at91_cpu_is(u_int cpu)
{

	return (soc_info.type == cpu);
}

void at91_add_child(device_t dev, int prio, const char *name, int unit,
    bus_addr_t addr, bus_size_t size, int irq0, int irq1, int irq2);

extern uint32_t at91_irq_system;
extern uint32_t at91_master_clock;
void at91_pmc_init_clock(void);

#endif /* _AT91VAR_H_ */
