/*-
 * Copyright (c) 2002, 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * from: FreeBSD: //depot/projects/arm/src/sys/arm/xscale/pxa2x0/pxa2x0var.h, rev 1
 *
 * $FreeBSD$
 */

#ifndef _MVVAR_H_
#define _MVVAR_H_

#include <sys/rman.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/pmap.h>
#include <machine/vm.h>

#define	MV_TYPE_PCI		0
#define	MV_TYPE_PCIE		1
#define	MV_TYPE_PCIE_AGGR_LANE	2	/* Additional PCIE lane to aggregate */

struct obio_softc {
	bus_space_tag_t obio_bst;	/* bus space tag */
	struct rman	obio_mem;
	struct rman	obio_irq;
	struct rman	obio_gpio;
};

struct obio_device {
	const char	*od_name;
	u_long		od_base;
	u_long		od_size;
	u_int		od_irqs[7 + 1];	/* keep additional entry for -1 sentinel */
	u_int		od_gpio[2 + 1]; /* as above for IRQ */
	u_int		od_pwr_mask;
	struct resource_list od_resources;
};

struct obio_pci_irq_map {
	int		opim_slot;
	int		opim_pin;
	int		opim_irq;
};

struct obio_pci {
	int		op_type;

	bus_addr_t	op_base;
	u_long		op_size;

	/* Note IO/MEM regions are assumed VA == PA */
	bus_addr_t	op_io_base;
	u_long		op_io_size;
	int		op_io_win_target;
	int		op_io_win_attr;

	bus_addr_t	op_mem_base;
	u_long		op_mem_size;
	int		op_mem_win_target;
	int		op_mem_win_attr;

	const struct obio_pci_irq_map	*op_pci_irq_map;
	int		op_irq;		/* used if IRQ map table is NULL */
};

struct gpio_config {
	int		gc_gpio;	/* GPIO number */
	uint32_t	gc_flags;	/* GPIO flags */
	int		gc_output;	/* GPIO output value */
};

struct decode_win {
	int		target;		/* Mbus unit ID */
	int		attr;		/* Attributes of the target interface */
	vm_paddr_t	base;		/* Physical base addr */
	uint32_t	size;
	int		remap;
};

extern const struct pmap_devmap pmap_devmap[];
extern const struct obio_pci mv_pci_info[];
extern const struct gpio_config mv_gpio_config[];
extern bus_space_tag_t obio_tag;
extern struct obio_device obio_devices[];
extern const struct decode_win *cpu_wins;
extern const struct decode_win *idma_wins;
extern const struct decode_win *xor_wins;
extern int cpu_wins_no;
extern int idma_wins_no;
extern int xor_wins_no;

/* Function prototypes */
int mv_gpio_setup_intrhandler(const char *name, driver_filter_t *filt,
    void (*hand)(void *), void *arg, int pin, int flags, void **cookiep);
void mv_gpio_intr_mask(int pin);
void mv_gpio_intr_unmask(int pin);
int mv_gpio_configure(uint32_t pin, uint32_t flags, uint32_t mask);
void mv_gpio_out(uint32_t pin, uint8_t val, uint8_t enable);
uint8_t mv_gpio_in(uint32_t pin);

void platform_mpp_init(void);
int soc_decode_win(void);
void soc_id(uint32_t *dev, uint32_t *rev);
void soc_identify(void);
void soc_dump_decode_win(void);
uint32_t soc_power_ctrl_get(uint32_t mask);
void soc_power_ctrl_set(uint32_t mask);

int decode_win_cpu_set(int target, int attr, vm_paddr_t base, uint32_t size,
    int remap);
int decode_win_overlap(int, int, const struct decode_win *);
int win_cpu_can_remap(int);

void decode_win_idma_dump(void);
void decode_win_idma_setup(void);
int decode_win_idma_valid(void);

void decode_win_xor_dump(void);
void decode_win_xor_setup(void);
int decode_win_xor_valid(void);

int ddr_is_active(int i);
uint32_t ddr_base(int i);
uint32_t ddr_size(int i);
uint32_t ddr_attr(int i);
uint32_t ddr_target(int i);

uint32_t cpu_extra_feat(void);
uint32_t get_tclk(void);
uint32_t read_cpu_ctrl(uint32_t);
void write_cpu_ctrl(uint32_t, uint32_t);

enum mbus_device_ivars {
	MBUS_IVAR_BASE,
};

#define	MBUS_ACCESSOR(var, ivar, type)	\
	__BUS_ACCESSOR(mbus, var, MBUS, ivar, type)

MBUS_ACCESSOR(base,	BASE,	u_long)

#undef	MBUS_ACCESSOR

#endif /* _MVVAR_H_ */
