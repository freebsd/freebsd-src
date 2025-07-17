/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (C) 2018 Alexandru Elisei <alexandru.elisei@gmail.com>
 * Copyright (C) 2020-2022 Andrew Turner
 * Copyright (C) 2023 Arm Ltd
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

#include <sys/cdefs.h>

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include <sys/bitstring.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/smp.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <dev/ofw/openfirm.h>

#include <machine/armreg.h>
#include <machine/atomic.h>
#include <machine/bus.h>
#include <machine/cpufunc.h>
#include <machine/cpu.h>
#include <machine/machdep.h>
#include <machine/param.h>
#include <machine/pmap.h>
#include <machine/vmparam.h>
#include <machine/intr.h>
#include <machine/vmm.h>
#include <machine/vmm_dev.h>
#include <machine/vmm_instruction_emul.h>

#include <arm/arm/gic_common.h>
#include <arm64/arm64/gic_v3_reg.h>
#include <arm64/arm64/gic_v3_var.h>

#include <arm64/vmm/hyp.h>
#include <arm64/vmm/mmu.h>
#include <arm64/vmm/arm64.h>
#include <arm64/vmm/vmm_handlers.h>

#include "vgic.h"
#include "vgic_v3.h"
#include "vgic_v3_reg.h"

#include "vgic_if.h"

#define VGIC_SGI_NUM		(GIC_LAST_SGI - GIC_FIRST_SGI + 1)
#define VGIC_PPI_NUM		(GIC_LAST_PPI - GIC_FIRST_PPI + 1)
#define VGIC_SPI_NUM		(GIC_LAST_SPI - GIC_FIRST_SPI + 1)
#define VGIC_PRV_I_NUM		(VGIC_SGI_NUM + VGIC_PPI_NUM)
#define VGIC_SHR_I_NUM		(VGIC_SPI_NUM)

MALLOC_DEFINE(M_VGIC_V3, "ARM VMM VGIC V3", "ARM VMM VGIC V3");

/* TODO: Move to softc */
struct vgic_v3_virt_features {
	uint8_t min_prio;
	size_t ich_lr_num;
	size_t ich_apr_num;
};

struct vgic_v3_irq {
	/* List of IRQs that are active or pending */
	TAILQ_ENTRY(vgic_v3_irq) act_pend_list;
	struct mtx irq_spinmtx;
	uint64_t mpidr;
	int target_vcpu;
	uint32_t irq;
	bool active;
	bool pending;
	bool enabled;
	bool level;
	bool on_aplist;
	uint8_t priority;
	uint8_t config;
#define	VGIC_CONFIG_MASK	0x2
#define	VGIC_CONFIG_LEVEL	0x0
#define	VGIC_CONFIG_EDGE	0x2
};

/* Global data not needed by EL2 */
struct vgic_v3 {
	struct mtx 	dist_mtx;
	uint64_t 	dist_start;
	size_t   	dist_end;

	uint64_t 	redist_start;
	size_t 		redist_end;

	uint32_t 	gicd_ctlr;	/* Distributor Control Register */

	struct vgic_v3_irq *irqs;
};

/* Per-CPU data not needed by EL2 */
struct vgic_v3_cpu {
	/*
	 * We need a mutex for accessing the list registers because they are
	 * modified asynchronously by the virtual timer.
	 *
	 * Note that the mutex *MUST* be a spin mutex because an interrupt can
	 * be injected by a callout callback function, thereby modifying the
	 * list registers from a context where sleeping is forbidden.
	 */
	struct mtx	lr_mtx;

	struct vgic_v3_irq private_irqs[VGIC_PRV_I_NUM];
	TAILQ_HEAD(, vgic_v3_irq) irq_act_pend;
	u_int		ich_lr_used;
};

/* How many IRQs we support (SGIs + PPIs + SPIs). Not including LPIs */
#define	VGIC_NIRQS	1023
/* Pretend to be an Arm design */
#define	VGIC_IIDR	0x43b

static vgic_inject_irq_t vgic_v3_inject_irq;
static vgic_inject_msi_t vgic_v3_inject_msi;

static int vgic_v3_max_cpu_count(device_t dev, struct hyp *hyp);

#define	INJECT_IRQ(hyp, vcpuid, irqid, level)			\
    vgic_v3_inject_irq(NULL, (hyp), (vcpuid), (irqid), (level))

typedef void (register_read)(struct hypctx *, u_int, uint64_t *, void *);
typedef void (register_write)(struct hypctx *, u_int, u_int, u_int,
    uint64_t, void *);

#define	VGIC_8_BIT	(1 << 0)
/* (1 << 1) is reserved for 16 bit accesses */
#define	VGIC_32_BIT	(1 << 2)
#define	VGIC_64_BIT	(1 << 3)

struct vgic_register {
	u_int start;	/* Start within a memory region */
	u_int end;
	u_int size;
	u_int flags;
	register_read *read;
	register_write *write;
};

#define	VGIC_REGISTER_RANGE(reg_start, reg_end, reg_size, reg_flags, readf, \
    writef)								\
{									\
	.start = (reg_start),						\
	.end = (reg_end),						\
	.size = (reg_size),						\
	.flags = (reg_flags),						\
	.read = (readf),						\
	.write = (writef),						\
}

#define	VGIC_REGISTER_RANGE_RAZ_WI(reg_start, reg_end, reg_size, reg_flags) \
	VGIC_REGISTER_RANGE(reg_start, reg_end, reg_size, reg_flags,	\
	    gic_zero_read, gic_ignore_write)

#define	VGIC_REGISTER(start_addr, reg_size, reg_flags, readf, writef)	\
	VGIC_REGISTER_RANGE(start_addr, (start_addr) + (reg_size),	\
	    reg_size, reg_flags, readf, writef)

#define	VGIC_REGISTER_RAZ_WI(start_addr, reg_size, reg_flags)		\
	VGIC_REGISTER_RANGE_RAZ_WI(start_addr,				\
	    (start_addr) + (reg_size), reg_size, reg_flags)

static register_read gic_pidr2_read;
static register_read gic_zero_read;
static register_write gic_ignore_write;

/* GICD_CTLR */
static register_read dist_ctlr_read;
static register_write dist_ctlr_write;
/* GICD_TYPER */
static register_read dist_typer_read;
/* GICD_IIDR */
static register_read dist_iidr_read;
/* GICD_STATUSR - RAZ/WI as we don't report errors (yet) */
/* GICD_SETSPI_NSR & GICD_CLRSPI_NSR */
static register_write dist_setclrspi_nsr_write;
/* GICD_SETSPI_SR - RAZ/WI */
/* GICD_CLRSPI_SR - RAZ/WI */
/* GICD_IGROUPR - RAZ/WI as GICD_CTLR.ARE == 1 */
/* GICD_ISENABLER */
static register_read dist_isenabler_read;
static register_write dist_isenabler_write;
/* GICD_ICENABLER */
static register_read dist_icenabler_read;
static register_write dist_icenabler_write;
/* GICD_ISPENDR */
static register_read dist_ispendr_read;
static register_write dist_ispendr_write;
/* GICD_ICPENDR */
static register_read dist_icpendr_read;
static register_write dist_icpendr_write;
/* GICD_ISACTIVER */
static register_read dist_isactiver_read;
static register_write dist_isactiver_write;
/* GICD_ICACTIVER */
static register_read dist_icactiver_read;
static register_write dist_icactiver_write;
/* GICD_IPRIORITYR */
static register_read dist_ipriorityr_read;
static register_write dist_ipriorityr_write;
/* GICD_ITARGETSR - RAZ/WI as GICD_CTLR.ARE == 1 */
/* GICD_ICFGR */
static register_read dist_icfgr_read;
static register_write dist_icfgr_write;
/* GICD_IGRPMODR - RAZ/WI from non-secure mode */
/* GICD_NSACR - RAZ/WI from non-secure mode */
/* GICD_SGIR - RAZ/WI as GICD_CTLR.ARE == 1 */
/* GICD_CPENDSGIR - RAZ/WI as GICD_CTLR.ARE == 1 */
/* GICD_SPENDSGIR - RAZ/WI as GICD_CTLR.ARE == 1 */
/* GICD_IROUTER */
static register_read dist_irouter_read;
static register_write dist_irouter_write;

static struct vgic_register dist_registers[] = {
	VGIC_REGISTER(GICD_CTLR, 4, VGIC_32_BIT, dist_ctlr_read,
	    dist_ctlr_write),
	VGIC_REGISTER(GICD_TYPER, 4, VGIC_32_BIT, dist_typer_read,
	    gic_ignore_write),
	VGIC_REGISTER(GICD_IIDR, 4, VGIC_32_BIT, dist_iidr_read,
	    gic_ignore_write),
	VGIC_REGISTER_RAZ_WI(GICD_STATUSR, 4, VGIC_32_BIT),
	VGIC_REGISTER(GICD_SETSPI_NSR, 4, VGIC_32_BIT, gic_zero_read,
	    dist_setclrspi_nsr_write),
	VGIC_REGISTER(GICD_CLRSPI_NSR, 4, VGIC_32_BIT, gic_zero_read,
	    dist_setclrspi_nsr_write),
	VGIC_REGISTER_RAZ_WI(GICD_SETSPI_SR, 4, VGIC_32_BIT),
	VGIC_REGISTER_RAZ_WI(GICD_CLRSPI_SR, 4, VGIC_32_BIT),
	VGIC_REGISTER_RANGE_RAZ_WI(GICD_IGROUPR(0), GICD_IGROUPR(1024), 4,
	    VGIC_32_BIT),

	VGIC_REGISTER_RAZ_WI(GICD_ISENABLER(0), 4, VGIC_32_BIT),
	VGIC_REGISTER_RANGE(GICD_ISENABLER(32), GICD_ISENABLER(1024), 4,
	    VGIC_32_BIT, dist_isenabler_read, dist_isenabler_write),

	VGIC_REGISTER_RAZ_WI(GICD_ICENABLER(0), 4, VGIC_32_BIT),
	VGIC_REGISTER_RANGE(GICD_ICENABLER(32), GICD_ICENABLER(1024), 4,
	    VGIC_32_BIT, dist_icenabler_read, dist_icenabler_write),

	VGIC_REGISTER_RAZ_WI(GICD_ISPENDR(0), 4, VGIC_32_BIT),
	VGIC_REGISTER_RANGE(GICD_ISPENDR(32), GICD_ISPENDR(1024), 4,
	    VGIC_32_BIT, dist_ispendr_read, dist_ispendr_write),

	VGIC_REGISTER_RAZ_WI(GICD_ICPENDR(0), 4, VGIC_32_BIT),
	VGIC_REGISTER_RANGE(GICD_ICPENDR(32), GICD_ICPENDR(1024), 4,
	    VGIC_32_BIT, dist_icpendr_read, dist_icpendr_write),

	VGIC_REGISTER_RAZ_WI(GICD_ISACTIVER(0), 4, VGIC_32_BIT),
	VGIC_REGISTER_RANGE(GICD_ISACTIVER(32), GICD_ISACTIVER(1024), 4,
	    VGIC_32_BIT, dist_isactiver_read, dist_isactiver_write),

	VGIC_REGISTER_RAZ_WI(GICD_ICACTIVER(0), 4, VGIC_32_BIT),
	VGIC_REGISTER_RANGE(GICD_ICACTIVER(32), GICD_ICACTIVER(1024), 4,
	    VGIC_32_BIT, dist_icactiver_read, dist_icactiver_write),

	VGIC_REGISTER_RANGE_RAZ_WI(GICD_IPRIORITYR(0), GICD_IPRIORITYR(32), 4,
	    VGIC_32_BIT | VGIC_8_BIT),
	VGIC_REGISTER_RANGE(GICD_IPRIORITYR(32), GICD_IPRIORITYR(1024), 4,
	    VGIC_32_BIT | VGIC_8_BIT, dist_ipriorityr_read,
	    dist_ipriorityr_write),

	VGIC_REGISTER_RANGE_RAZ_WI(GICD_ITARGETSR(0), GICD_ITARGETSR(1024), 4,
	    VGIC_32_BIT | VGIC_8_BIT),

	VGIC_REGISTER_RANGE_RAZ_WI(GICD_ICFGR(0), GICD_ICFGR(32), 4,
	    VGIC_32_BIT),
	VGIC_REGISTER_RANGE(GICD_ICFGR(32), GICD_ICFGR(1024), 4,
	    VGIC_32_BIT, dist_icfgr_read, dist_icfgr_write),
/*
	VGIC_REGISTER_RANGE(GICD_IGRPMODR(0), GICD_IGRPMODR(1024), 4,
	    VGIC_32_BIT, dist_igrpmodr_read, dist_igrpmodr_write),
	VGIC_REGISTER_RANGE(GICD_NSACR(0), GICD_NSACR(1024), 4,
	    VGIC_32_BIT, dist_nsacr_read, dist_nsacr_write),
*/
	VGIC_REGISTER_RAZ_WI(GICD_SGIR, 4, VGIC_32_BIT),
/*
	VGIC_REGISTER_RANGE(GICD_CPENDSGIR(0), GICD_CPENDSGIR(1024), 4,
	    VGIC_32_BIT | VGIC_8_BIT, dist_cpendsgir_read,
	    dist_cpendsgir_write),
	VGIC_REGISTER_RANGE(GICD_SPENDSGIR(0), GICD_SPENDSGIR(1024), 4,
	    VGIC_32_BIT | VGIC_8_BIT, dist_spendsgir_read,
	    dist_spendsgir_write),
*/
	VGIC_REGISTER_RANGE(GICD_IROUTER(32), GICD_IROUTER(1024), 8,
	    VGIC_64_BIT | VGIC_32_BIT, dist_irouter_read, dist_irouter_write),

	VGIC_REGISTER_RANGE_RAZ_WI(GICD_PIDR4, GICD_PIDR2, 4, VGIC_32_BIT),
	VGIC_REGISTER(GICD_PIDR2, 4, VGIC_32_BIT, gic_pidr2_read,
	    gic_ignore_write),
	VGIC_REGISTER_RANGE_RAZ_WI(GICD_PIDR2 + 4, GICD_SIZE, 4, VGIC_32_BIT),
};

/* GICR_CTLR - Ignore writes as no bits can be set */
static register_read redist_ctlr_read;
/* GICR_IIDR */
static register_read redist_iidr_read;
/* GICR_TYPER */
static register_read redist_typer_read;
/* GICR_STATUSR - RAZ/WI as we don't report errors (yet) */
/* GICR_WAKER - RAZ/WI from non-secure mode */
/* GICR_SETLPIR - RAZ/WI as no LPIs are supported */
/* GICR_CLRLPIR - RAZ/WI as no LPIs are supported */
/* GICR_PROPBASER - RAZ/WI as no LPIs are supported */
/* GICR_PENDBASER - RAZ/WI as no LPIs are supported */
/* GICR_INVLPIR - RAZ/WI as no LPIs are supported */
/* GICR_INVALLR - RAZ/WI as no LPIs are supported */
/* GICR_SYNCR - RAZ/WI as no LPIs are supported */

static struct vgic_register redist_rd_registers[] = {
	VGIC_REGISTER(GICR_CTLR, 4, VGIC_32_BIT, redist_ctlr_read,
	    gic_ignore_write),
	VGIC_REGISTER(GICR_IIDR, 4, VGIC_32_BIT, redist_iidr_read,
	    gic_ignore_write),
	VGIC_REGISTER(GICR_TYPER, 8, VGIC_64_BIT | VGIC_32_BIT,
	    redist_typer_read, gic_ignore_write),
	VGIC_REGISTER_RAZ_WI(GICR_STATUSR, 4, VGIC_32_BIT),
	VGIC_REGISTER_RAZ_WI(GICR_WAKER, 4, VGIC_32_BIT),
	VGIC_REGISTER_RAZ_WI(GICR_SETLPIR, 8, VGIC_64_BIT | VGIC_32_BIT),
	VGIC_REGISTER_RAZ_WI(GICR_CLRLPIR, 8, VGIC_64_BIT | VGIC_32_BIT),
	VGIC_REGISTER_RAZ_WI(GICR_PROPBASER, 8, VGIC_64_BIT | VGIC_32_BIT),
	VGIC_REGISTER_RAZ_WI(GICR_PENDBASER, 8, VGIC_64_BIT | VGIC_32_BIT),
	VGIC_REGISTER_RAZ_WI(GICR_INVLPIR, 8, VGIC_64_BIT | VGIC_32_BIT),
	VGIC_REGISTER_RAZ_WI(GICR_INVALLR, 8, VGIC_64_BIT | VGIC_32_BIT),
	VGIC_REGISTER_RAZ_WI(GICR_SYNCR, 4, VGIC_32_BIT),

	/* These are identical to the dist registers */
	VGIC_REGISTER_RANGE_RAZ_WI(GICD_PIDR4, GICD_PIDR2, 4, VGIC_32_BIT),
	VGIC_REGISTER(GICD_PIDR2, 4, VGIC_32_BIT, gic_pidr2_read,
	    gic_ignore_write),
	VGIC_REGISTER_RANGE_RAZ_WI(GICD_PIDR2 + 4, GICD_SIZE, 4,
	    VGIC_32_BIT),
};

/* GICR_IGROUPR0 - RAZ/WI from non-secure mode */
/* GICR_ISENABLER0 */
static register_read redist_ienabler0_read;
static register_write redist_isenabler0_write;
/* GICR_ICENABLER0 */
static register_write redist_icenabler0_write;
/* GICR_ISPENDR0 */
static register_read redist_ipendr0_read;
static register_write redist_ispendr0_write;
/* GICR_ICPENDR0 */
static register_write redist_icpendr0_write;
/* GICR_ISACTIVER0 */
static register_read redist_iactiver0_read;
static register_write redist_isactiver0_write;
/* GICR_ICACTIVER0 */
static register_write redist_icactiver0_write;
/* GICR_IPRIORITYR */
static register_read redist_ipriorityr_read;
static register_write redist_ipriorityr_write;
/* GICR_ICFGR0 - RAZ/WI from non-secure mode */
/* GICR_ICFGR1 */
static register_read redist_icfgr1_read;
static register_write redist_icfgr1_write;
/* GICR_IGRPMODR0 - RAZ/WI from non-secure mode */
/* GICR_NSCAR - RAZ/WI from non-secure mode */

static struct vgic_register redist_sgi_registers[] = {
	VGIC_REGISTER_RAZ_WI(GICR_IGROUPR0, 4, VGIC_32_BIT),
	VGIC_REGISTER(GICR_ISENABLER0, 4, VGIC_32_BIT, redist_ienabler0_read,
	    redist_isenabler0_write),
	VGIC_REGISTER(GICR_ICENABLER0, 4, VGIC_32_BIT, redist_ienabler0_read,
	    redist_icenabler0_write),
	VGIC_REGISTER(GICR_ISPENDR0, 4, VGIC_32_BIT, redist_ipendr0_read,
	    redist_ispendr0_write),
	VGIC_REGISTER(GICR_ICPENDR0, 4, VGIC_32_BIT, redist_ipendr0_read,
	    redist_icpendr0_write),
	VGIC_REGISTER(GICR_ISACTIVER0, 4, VGIC_32_BIT, redist_iactiver0_read,
	    redist_isactiver0_write),
	VGIC_REGISTER(GICR_ICACTIVER0, 4, VGIC_32_BIT, redist_iactiver0_read,
	    redist_icactiver0_write),
	VGIC_REGISTER_RANGE(GICR_IPRIORITYR(0), GICR_IPRIORITYR(32), 4,
	    VGIC_32_BIT | VGIC_8_BIT, redist_ipriorityr_read,
	    redist_ipriorityr_write),
	VGIC_REGISTER_RAZ_WI(GICR_ICFGR0, 4, VGIC_32_BIT),
	VGIC_REGISTER(GICR_ICFGR1, 4, VGIC_32_BIT, redist_icfgr1_read,
	    redist_icfgr1_write),
	VGIC_REGISTER_RAZ_WI(GICR_IGRPMODR0, 4, VGIC_32_BIT),
	VGIC_REGISTER_RAZ_WI(GICR_NSACR, 4, VGIC_32_BIT),
};

static struct vgic_v3_virt_features virt_features;

static struct vgic_v3_irq *vgic_v3_get_irq(struct hyp *, int, uint32_t);
static void vgic_v3_release_irq(struct vgic_v3_irq *);

/* TODO: Move to a common file */
static int
mpidr_to_vcpu(struct hyp *hyp, uint64_t mpidr)
{
	struct vm *vm;
	struct hypctx *hypctx;

	vm = hyp->vm;
	for (int i = 0; i < vm_get_maxcpus(vm); i++) {
		hypctx = hyp->ctx[i];
		if (hypctx != NULL && (hypctx->vmpidr_el2 & GICD_AFF) == mpidr)
			return (i);
	}
	return (-1);
}

static void
vgic_v3_vminit(device_t dev, struct hyp *hyp)
{
	struct vgic_v3 *vgic;

	hyp->vgic = malloc(sizeof(*hyp->vgic), M_VGIC_V3,
	    M_WAITOK | M_ZERO);
	vgic = hyp->vgic;

	/*
	 * Configure the Distributor control register. The register resets to an
	 * architecturally UNKNOWN value, so we reset to 0 to disable all
	 * functionality controlled by the register.
	 *
	 * The exception is GICD_CTLR.DS, which is RA0/WI when the Distributor
	 * supports one security state (ARM GIC Architecture Specification for
	 * GICv3 and GICv4, p. 4-464)
	 */
	vgic->gicd_ctlr = 0;

	mtx_init(&vgic->dist_mtx, "VGICv3 Distributor lock", NULL,
	    MTX_SPIN);
}

static void
vgic_v3_cpuinit(device_t dev, struct hypctx *hypctx)
{
	struct vgic_v3_cpu *vgic_cpu;
	struct vgic_v3_irq *irq;
	int i, irqid;

	hypctx->vgic_cpu = malloc(sizeof(*hypctx->vgic_cpu),
	    M_VGIC_V3, M_WAITOK | M_ZERO);
	vgic_cpu = hypctx->vgic_cpu;

	mtx_init(&vgic_cpu->lr_mtx, "VGICv3 ICH_LR_EL2 lock", NULL, MTX_SPIN);

	/* Set the SGI and PPI state */
	for (irqid = 0; irqid < VGIC_PRV_I_NUM; irqid++) {
		irq = &vgic_cpu->private_irqs[irqid];

		mtx_init(&irq->irq_spinmtx, "VGIC IRQ spinlock", NULL,
		    MTX_SPIN);
		irq->irq = irqid;
		irq->mpidr = hypctx->vmpidr_el2 & GICD_AFF;
		irq->target_vcpu = vcpu_vcpuid(hypctx->vcpu);
		MPASS(irq->target_vcpu >= 0);

		if (irqid < VGIC_SGI_NUM) {
			/* SGIs */
			irq->enabled = true;
			irq->config = VGIC_CONFIG_EDGE;
		} else {
			/* PPIs */
			irq->config = VGIC_CONFIG_LEVEL;
		}
		irq->priority = 0;
	}

	/*
	 * Configure the Interrupt Controller Hyp Control Register.
	 *
	 * ICH_HCR_EL2_En: enable virtual CPU interface.
	 *
	 * Maintenance interrupts are disabled.
	 */
	hypctx->vgic_v3_regs.ich_hcr_el2 = ICH_HCR_EL2_En;

	/*
	 * Configure the Interrupt Controller Virtual Machine Control Register.
	 *
	 * ICH_VMCR_EL2_VPMR: lowest priority mask for the VCPU interface
	 * ICH_VMCR_EL2_VBPR1_NO_PREEMPTION: disable interrupt preemption for
	 * Group 1 interrupts
	 * ICH_VMCR_EL2_VBPR0_NO_PREEMPTION: disable interrupt preemption for
	 * Group 0 interrupts
	 * ~ICH_VMCR_EL2_VEOIM: writes to EOI registers perform priority drop
	 * and interrupt deactivation.
	 * ICH_VMCR_EL2_VENG0: virtual Group 0 interrupts enabled.
	 * ICH_VMCR_EL2_VENG1: virtual Group 1 interrupts enabled.
	 */
	hypctx->vgic_v3_regs.ich_vmcr_el2 =
	    (virt_features.min_prio << ICH_VMCR_EL2_VPMR_SHIFT) |
	    ICH_VMCR_EL2_VBPR1_NO_PREEMPTION | ICH_VMCR_EL2_VBPR0_NO_PREEMPTION;
	hypctx->vgic_v3_regs.ich_vmcr_el2 &= ~ICH_VMCR_EL2_VEOIM;
	hypctx->vgic_v3_regs.ich_vmcr_el2 |= ICH_VMCR_EL2_VENG0 |
	    ICH_VMCR_EL2_VENG1;

	hypctx->vgic_v3_regs.ich_lr_num = virt_features.ich_lr_num;
	for (i = 0; i < hypctx->vgic_v3_regs.ich_lr_num; i++)
		hypctx->vgic_v3_regs.ich_lr_el2[i] = 0UL;
	vgic_cpu->ich_lr_used = 0;
	TAILQ_INIT(&vgic_cpu->irq_act_pend);

	hypctx->vgic_v3_regs.ich_apr_num = virt_features.ich_apr_num;
}

static void
vgic_v3_cpucleanup(device_t dev, struct hypctx *hypctx)
{
	struct vgic_v3_cpu *vgic_cpu;
	struct vgic_v3_irq *irq;
	int irqid;

	vgic_cpu = hypctx->vgic_cpu;
	for (irqid = 0; irqid < VGIC_PRV_I_NUM; irqid++) {
		irq = &vgic_cpu->private_irqs[irqid];
		mtx_destroy(&irq->irq_spinmtx);
	}

	mtx_destroy(&vgic_cpu->lr_mtx);
	free(hypctx->vgic_cpu, M_VGIC_V3);
}

static void
vgic_v3_vmcleanup(device_t dev, struct hyp *hyp)
{
	mtx_destroy(&hyp->vgic->dist_mtx);
	free(hyp->vgic, M_VGIC_V3);
}

static int
vgic_v3_max_cpu_count(device_t dev, struct hyp *hyp)
{
	struct vgic_v3 *vgic;
	size_t count;
	int16_t max_count;

	vgic = hyp->vgic;
	max_count = vm_get_maxcpus(hyp->vm);

	/* No registers, assume the maximum CPUs */
	if (vgic->redist_start == 0 && vgic->redist_end == 0)
		return (max_count);

	count = (vgic->redist_end - vgic->redist_start) /
	    (GICR_RD_BASE_SIZE + GICR_SGI_BASE_SIZE);

	/*
	 * max_count is smaller than INT_MAX so will also limit count
	 * to a positive integer value.
	 */
	if (count > max_count)
		return (max_count);

	return (count);
}

static bool
vgic_v3_irq_pending(struct vgic_v3_irq *irq)
{
	if ((irq->config & VGIC_CONFIG_MASK) == VGIC_CONFIG_LEVEL) {
		return (irq->pending || irq->level);
	} else {
		return (irq->pending);
	}
}

static bool
vgic_v3_queue_irq(struct hyp *hyp, struct vgic_v3_cpu *vgic_cpu,
    int vcpuid, struct vgic_v3_irq *irq)
{
	MPASS(vcpuid >= 0);
	MPASS(vcpuid < vm_get_maxcpus(hyp->vm));

	mtx_assert(&vgic_cpu->lr_mtx, MA_OWNED);
	mtx_assert(&irq->irq_spinmtx, MA_OWNED);

	/* No need to queue the IRQ */
	if (!irq->level && !irq->pending)
		return (false);

	if (!irq->on_aplist) {
		irq->on_aplist = true;
		TAILQ_INSERT_TAIL(&vgic_cpu->irq_act_pend, irq, act_pend_list);
	}
	return (true);
}

static uint64_t
gic_reg_value_64(uint64_t field, uint64_t val, u_int offset, u_int size)
{
	uint32_t mask;

	if (offset != 0 || size != 8) {
		mask = ((1ul << (size * 8)) - 1) << (offset * 8);
		/* Shift the new bits to the correct place */
		val <<= (offset * 8);
		/* Keep only the interesting bits */
		val &= mask;
		/* Add the bits we are keeping from the old value */
		val |= field & ~mask;
	}

	return (val);
}

static void
gic_pidr2_read(struct hypctx *hypctx, u_int reg, uint64_t *rval,
    void *arg)
{
	*rval = GICR_PIDR2_ARCH_GICv3 << GICR_PIDR2_ARCH_SHIFT;
}

/* Common read-only/write-ignored helpers */
static void
gic_zero_read(struct hypctx *hypctx, u_int reg, uint64_t *rval,
    void *arg)
{
	*rval = 0;
}

static void
gic_ignore_write(struct hypctx *hypctx, u_int reg, u_int offset, u_int size,
    uint64_t wval, void *arg)
{
	/* Nothing to do */
}

static uint64_t
read_enabler(struct hypctx *hypctx, int n)
{
	struct vgic_v3_irq *irq;
	uint64_t ret;
	uint32_t irq_base;
	int i;

	ret = 0;
	irq_base = n * 32;
	for (i = 0; i < 32; i++) {
		irq = vgic_v3_get_irq(hypctx->hyp, vcpu_vcpuid(hypctx->vcpu),
		    irq_base + i);
		if (irq == NULL)
			continue;

		if (!irq->enabled)
			ret |= 1u << i;
		vgic_v3_release_irq(irq);
	}

	return (ret);
}

static void
write_enabler(struct hypctx *hypctx,int n, bool set, uint64_t val)
{
	struct vgic_v3_irq *irq;
	uint32_t irq_base;
	int i;

	irq_base = n * 32;
	for (i = 0; i < 32; i++) {
		/* We only change interrupts when the appropriate bit is set */
		if ((val & (1u << i)) == 0)
			continue;

		/* Find the interrupt this bit represents */
		irq = vgic_v3_get_irq(hypctx->hyp, vcpu_vcpuid(hypctx->vcpu),
		    irq_base + i);
		if (irq == NULL)
			continue;

		irq->enabled = set;
		vgic_v3_release_irq(irq);
	}
}

static uint64_t
read_pendr(struct hypctx *hypctx, int n)
{
	struct vgic_v3_irq *irq;
	uint64_t ret;
	uint32_t irq_base;
	int i;

	ret = 0;
	irq_base = n * 32;
	for (i = 0; i < 32; i++) {
		irq = vgic_v3_get_irq(hypctx->hyp, vcpu_vcpuid(hypctx->vcpu),
		    irq_base + i);
		if (irq == NULL)
			continue;

		if (vgic_v3_irq_pending(irq))
			ret |= 1u << i;
		vgic_v3_release_irq(irq);
	}

	return (ret);
}

static uint64_t
write_pendr(struct hypctx *hypctx, int n, bool set, uint64_t val)
{
	struct vgic_v3_cpu *vgic_cpu;
	struct vgic_v3_irq *irq;
	struct hyp *hyp;
	struct hypctx *target_hypctx;
	uint64_t ret;
	uint32_t irq_base;
	int target_vcpu, i;
	bool notify;

	hyp = hypctx->hyp;
	ret = 0;
	irq_base = n * 32;
	for (i = 0; i < 32; i++) {
		/* We only change interrupts when the appropriate bit is set */
		if ((val & (1u << i)) == 0)
			continue;

		irq = vgic_v3_get_irq(hypctx->hyp, vcpu_vcpuid(hypctx->vcpu),
		    irq_base + i);
		if (irq == NULL)
			continue;

		notify = false;
		target_vcpu = irq->target_vcpu;
		if (target_vcpu < 0)
			goto next_irq;
		target_hypctx = hyp->ctx[target_vcpu];
		if (target_hypctx == NULL)
			goto next_irq;
		vgic_cpu = target_hypctx->vgic_cpu;

		if (!set) {
			/* pending -> not pending */
			irq->pending = false;
		} else {
			irq->pending = true;
			mtx_lock_spin(&vgic_cpu->lr_mtx);
			notify = vgic_v3_queue_irq(hyp, vgic_cpu, target_vcpu,
			    irq);
			mtx_unlock_spin(&vgic_cpu->lr_mtx);
		}
next_irq:
		vgic_v3_release_irq(irq);

		if (notify)
			vcpu_notify_event(vm_vcpu(hyp->vm, target_vcpu));
	}

	return (ret);
}

static uint64_t
read_activer(struct hypctx *hypctx, int n)
{
	struct vgic_v3_irq *irq;
	uint64_t ret;
	uint32_t irq_base;
	int i;

	ret = 0;
	irq_base = n * 32;
	for (i = 0; i < 32; i++) {
		irq = vgic_v3_get_irq(hypctx->hyp, vcpu_vcpuid(hypctx->vcpu),
		    irq_base + i);
		if (irq == NULL)
			continue;

		if (irq->active)
			ret |= 1u << i;
		vgic_v3_release_irq(irq);
	}

	return (ret);
}

static void
write_activer(struct hypctx *hypctx, u_int n, bool set, uint64_t val)
{
	struct vgic_v3_cpu *vgic_cpu;
	struct vgic_v3_irq *irq;
	struct hyp *hyp;
	struct hypctx *target_hypctx;
	uint32_t irq_base;
	int target_vcpu, i;
	bool notify;

	hyp = hypctx->hyp;
	irq_base = n * 32;
	for (i = 0; i < 32; i++) {
		/* We only change interrupts when the appropriate bit is set */
		if ((val & (1u << i)) == 0)
			continue;

		irq = vgic_v3_get_irq(hypctx->hyp, vcpu_vcpuid(hypctx->vcpu),
		    irq_base + i);
		if (irq == NULL)
			continue;

		notify = false;
		target_vcpu = irq->target_vcpu;
		if (target_vcpu < 0)
			goto next_irq;
		target_hypctx = hyp->ctx[target_vcpu];
		if (target_hypctx == NULL)
			goto next_irq;
		vgic_cpu = target_hypctx->vgic_cpu;

		if (!set) {
			/* active -> not active */
			irq->active = false;
		} else {
			/* not active -> active */
			irq->active = true;
			mtx_lock_spin(&vgic_cpu->lr_mtx);
			notify = vgic_v3_queue_irq(hyp, vgic_cpu, target_vcpu,
			    irq);
			mtx_unlock_spin(&vgic_cpu->lr_mtx);
		}
next_irq:
		vgic_v3_release_irq(irq);

		if (notify)
			vcpu_notify_event(vm_vcpu(hyp->vm, target_vcpu));
	}
}

static uint64_t
read_priorityr(struct hypctx *hypctx, int n)
{
	struct vgic_v3_irq *irq;
	uint64_t ret;
	uint32_t irq_base;
	int i;

	ret = 0;
	irq_base = n * 4;
	for (i = 0; i < 4; i++) {
		irq = vgic_v3_get_irq(hypctx->hyp, vcpu_vcpuid(hypctx->vcpu),
		    irq_base + i);
		if (irq == NULL)
			continue;

		ret |= ((uint64_t)irq->priority) << (i * 8);
		vgic_v3_release_irq(irq);
	}

	return (ret);
}

static void
write_priorityr(struct hypctx *hypctx, u_int irq_base, u_int size, uint64_t val)
{
	struct vgic_v3_irq *irq;
	int i;

	for (i = 0; i < size; i++) {
		irq = vgic_v3_get_irq(hypctx->hyp, vcpu_vcpuid(hypctx->vcpu),
		    irq_base + i);
		if (irq == NULL)
			continue;

		/* Set the priority. We support 32 priority steps (5 bits) */
		irq->priority = (val >> (i * 8)) & 0xf8;
		vgic_v3_release_irq(irq);
	}
}

static uint64_t
read_config(struct hypctx *hypctx, int n)
{
	struct vgic_v3_irq *irq;
	uint64_t ret;
	uint32_t irq_base;
	int i;

	ret = 0;
	irq_base = n * 16;
	for (i = 0; i < 16; i++) {
		irq = vgic_v3_get_irq(hypctx->hyp, vcpu_vcpuid(hypctx->vcpu),
		    irq_base + i);
		if (irq == NULL)
			continue;

		ret |= ((uint64_t)irq->config) << (i * 2);
		vgic_v3_release_irq(irq);
	}

	return (ret);
}

static void
write_config(struct hypctx *hypctx, int n, uint64_t val)
{
	struct vgic_v3_irq *irq;
	uint32_t irq_base;
	int i;

	irq_base = n * 16;
	for (i = 0; i < 16; i++) {
		/*
		 * The config can't be changed for SGIs and PPIs. SGIs have
		 * an edge-triggered behaviour, and the register is
		 * implementation defined to be read-only for PPIs.
		 */
		if (irq_base + i < VGIC_PRV_I_NUM)
			continue;

		irq = vgic_v3_get_irq(hypctx->hyp, vcpu_vcpuid(hypctx->vcpu),
		    irq_base + i);
		if (irq == NULL)
			continue;

		/* Bit 0 is RES0 */
		irq->config = (val >> (i * 2)) & VGIC_CONFIG_MASK;
		vgic_v3_release_irq(irq);
	}
}

static uint64_t
read_route(struct hypctx *hypctx, int n)
{
	struct vgic_v3_irq *irq;
	uint64_t mpidr;

	irq = vgic_v3_get_irq(hypctx->hyp, vcpu_vcpuid(hypctx->vcpu), n);
	if (irq == NULL)
		return (0);

	mpidr = irq->mpidr;
	vgic_v3_release_irq(irq);

	return (mpidr);
}

static void
write_route(struct hypctx *hypctx, int n, uint64_t val, u_int offset,
    u_int size)
{
	struct vgic_v3_irq *irq;

	irq = vgic_v3_get_irq(hypctx->hyp, vcpu_vcpuid(hypctx->vcpu), n);
	if (irq == NULL)
		return;

	irq->mpidr = gic_reg_value_64(irq->mpidr, val, offset, size) & GICD_AFF;
	irq->target_vcpu = mpidr_to_vcpu(hypctx->hyp, irq->mpidr);
	/*
	 * If the interrupt is pending we can either use the old mpidr, or
	 * the new mpidr. To simplify this code we use the old value so we
	 * don't need to move the interrupt until the next time it is
	 * moved to the pending state.
	 */
	vgic_v3_release_irq(irq);
}

/*
 * Distributor register handlers.
 */
/* GICD_CTLR */
static void
dist_ctlr_read(struct hypctx *hypctx, u_int reg, uint64_t *rval,
    void *arg)
{
	struct hyp *hyp;
	struct vgic_v3 *vgic;

	hyp = hypctx->hyp;
	vgic = hyp->vgic;

	mtx_lock_spin(&vgic->dist_mtx);
	*rval = vgic->gicd_ctlr;
	mtx_unlock_spin(&vgic->dist_mtx);

	/* Writes are never pending */
	*rval &= ~GICD_CTLR_RWP;
}

static void
dist_ctlr_write(struct hypctx *hypctx, u_int reg, u_int offset, u_int size,
    uint64_t wval, void *arg)
{
	struct vgic_v3 *vgic;

	MPASS(offset == 0);
	MPASS(size == 4);
	vgic = hypctx->hyp->vgic;

	/*
	 * GICv2 backwards compatibility is not implemented so
	 * ARE_NS is RAO/WI. This means EnableGrp1 is RES0.
	 *
	 * EnableGrp1A is supported, and RWP is read-only.
	 *
	 * All other bits are RES0 from non-secure mode as we
	 * implement as if we are in a system with two security
	 * states.
	 */
	wval &= GICD_CTLR_G1A;
	wval |= GICD_CTLR_ARE_NS;
	mtx_lock_spin(&vgic->dist_mtx);
	vgic->gicd_ctlr = wval;
	/* TODO: Wake any vcpus that have interrupts pending */
	mtx_unlock_spin(&vgic->dist_mtx);
}

/* GICD_TYPER */
static void
dist_typer_read(struct hypctx *hypctx, u_int reg, uint64_t *rval,
    void *arg)
{
	uint32_t typer;

	typer = (10 - 1) << GICD_TYPER_IDBITS_SHIFT;
	typer |= GICD_TYPER_MBIS;
	/* ITLinesNumber: */
	typer |= howmany(VGIC_NIRQS + 1, 32) - 1;

	*rval = typer;
}

/* GICD_IIDR */
static void
dist_iidr_read(struct hypctx *hypctx, u_int reg, uint64_t *rval, void *arg)
{
	*rval = VGIC_IIDR;
}

/* GICD_SETSPI_NSR & GICD_CLRSPI_NSR */
static void
dist_setclrspi_nsr_write(struct hypctx *hypctx, u_int reg, u_int offset,
    u_int size, uint64_t wval, void *arg)
{
	uint32_t irqid;

	MPASS(offset == 0);
	MPASS(size == 4);
	irqid = wval & GICD_SPI_INTID_MASK;
	INJECT_IRQ(hypctx->hyp, vcpu_vcpuid(hypctx->vcpu), irqid,
	    reg == GICD_SETSPI_NSR);
}

/* GICD_ISENABLER */
static void
dist_isenabler_read(struct hypctx *hypctx, u_int reg, uint64_t *rval, void *arg)
{
	int n;

	n = (reg - GICD_ISENABLER(0)) / 4;
	/* GICD_ISENABLER0 is RAZ/WI so handled separately */
	MPASS(n > 0);
	*rval = read_enabler(hypctx, n);
}

static void
dist_isenabler_write(struct hypctx *hypctx, u_int reg, u_int offset, u_int size,
    uint64_t wval, void *arg)
{
	int n;

	MPASS(offset == 0);
	MPASS(size == 4);
	n = (reg - GICD_ISENABLER(0)) / 4;
	/* GICD_ISENABLER0 is RAZ/WI so handled separately */
	MPASS(n > 0);
	write_enabler(hypctx, n, true, wval);
}

/* GICD_ICENABLER */
static void
dist_icenabler_read(struct hypctx *hypctx, u_int reg, uint64_t *rval, void *arg)
{
	int n;

	n = (reg - GICD_ICENABLER(0)) / 4;
	/* GICD_ICENABLER0 is RAZ/WI so handled separately */
	MPASS(n > 0);
	*rval = read_enabler(hypctx, n);
}

static void
dist_icenabler_write(struct hypctx *hypctx, u_int reg, u_int offset, u_int size,
    uint64_t wval, void *arg)
{
	int n;

	MPASS(offset == 0);
	MPASS(size == 4);
	n = (reg - GICD_ISENABLER(0)) / 4;
	/* GICD_ICENABLER0 is RAZ/WI so handled separately */
	MPASS(n > 0);
	write_enabler(hypctx, n, false, wval);
}

/* GICD_ISPENDR */
static void
dist_ispendr_read(struct hypctx *hypctx, u_int reg, uint64_t *rval, void *arg)
{
	int n;

	n = (reg - GICD_ISPENDR(0)) / 4;
	/* GICD_ISPENDR0 is RAZ/WI so handled separately */
	MPASS(n > 0);
	*rval = read_pendr(hypctx, n);
}

static void
dist_ispendr_write(struct hypctx *hypctx, u_int reg, u_int offset, u_int size,
    uint64_t wval, void *arg)
{
	int n;

	MPASS(offset == 0);
	MPASS(size == 4);
	n = (reg - GICD_ISPENDR(0)) / 4;
	/* GICD_ISPENDR0 is RAZ/WI so handled separately */
	MPASS(n > 0);
	write_pendr(hypctx, n, true, wval);
}

/* GICD_ICPENDR */
static void
dist_icpendr_read(struct hypctx *hypctx, u_int reg, uint64_t *rval, void *arg)
{
	int n;

	n = (reg - GICD_ICPENDR(0)) / 4;
	/* GICD_ICPENDR0 is RAZ/WI so handled separately */
	MPASS(n > 0);
	*rval = read_pendr(hypctx, n);
}

static void
dist_icpendr_write(struct hypctx *hypctx, u_int reg, u_int offset, u_int size,
    uint64_t wval, void *arg)
{
	int n;

	MPASS(offset == 0);
	MPASS(size == 4);
	n = (reg - GICD_ICPENDR(0)) / 4;
	/* GICD_ICPENDR0 is RAZ/WI so handled separately */
	MPASS(n > 0);
	write_pendr(hypctx, n, false, wval);
}

/* GICD_ISACTIVER */
/* Affinity routing is enabled so isactiver0 is RAZ/WI */
static void
dist_isactiver_read(struct hypctx *hypctx, u_int reg, uint64_t *rval, void *arg)
{
	int n;

	n = (reg - GICD_ISACTIVER(0)) / 4;
	/* GICD_ISACTIVER0 is RAZ/WI so handled separately */
	MPASS(n > 0);
	*rval = read_activer(hypctx, n);
}

static void
dist_isactiver_write(struct hypctx *hypctx, u_int reg, u_int offset, u_int size,
    uint64_t wval, void *arg)
{
	int n;

	MPASS(offset == 0);
	MPASS(size == 4);
	n = (reg - GICD_ISACTIVER(0)) / 4;
	/* GICD_ISACTIVE0 is RAZ/WI so handled separately */
	MPASS(n > 0);
	write_activer(hypctx, n, true, wval);
}

/* GICD_ICACTIVER */
static void
dist_icactiver_read(struct hypctx *hypctx, u_int reg, uint64_t *rval,
    void *arg)
{
	int n;

	n = (reg - GICD_ICACTIVER(0)) / 4;
	/* GICD_ICACTIVE0 is RAZ/WI so handled separately */
	MPASS(n > 0);
	*rval = read_activer(hypctx, n);
}

static void
dist_icactiver_write(struct hypctx *hypctx, u_int reg, u_int offset, u_int size,
    uint64_t wval, void *arg)
{
	int n;

	MPASS(offset == 0);
	MPASS(size == 4);
	n = (reg - GICD_ICACTIVER(0)) / 4;
	/* GICD_ICACTIVE0 is RAZ/WI so handled separately */
	MPASS(n > 0);
	write_activer(hypctx, n, false, wval);
}

/* GICD_IPRIORITYR */
/* Affinity routing is enabled so ipriorityr0-7 is RAZ/WI */
static void
dist_ipriorityr_read(struct hypctx *hypctx, u_int reg, uint64_t *rval,
    void *arg)
{
	int n;

	n = (reg - GICD_IPRIORITYR(0)) / 4;
	/* GICD_IPRIORITY0-7 is RAZ/WI so handled separately */
	MPASS(n > 7);
	*rval = read_priorityr(hypctx, n);
}

static void
dist_ipriorityr_write(struct hypctx *hypctx, u_int reg, u_int offset,
    u_int size, uint64_t wval, void *arg)
{
	u_int irq_base;

	irq_base = (reg - GICD_IPRIORITYR(0)) + offset;
	/* GICD_IPRIORITY0-7 is RAZ/WI so handled separately */
	MPASS(irq_base > 31);
	write_priorityr(hypctx, irq_base, size, wval);
}

/* GICD_ICFGR */
static void
dist_icfgr_read(struct hypctx *hypctx, u_int reg, uint64_t *rval, void *arg)
{
	int n;

	n = (reg - GICD_ICFGR(0)) / 4;
	/* GICD_ICFGR0-1 are RAZ/WI so handled separately */
	MPASS(n > 1);
	*rval = read_config(hypctx, n);
}

static void
dist_icfgr_write(struct hypctx *hypctx, u_int reg, u_int offset, u_int size,
    uint64_t wval, void *arg)
{
	int n;

	MPASS(offset == 0);
	MPASS(size == 4);
	n = (reg - GICD_ICFGR(0)) / 4;
	/* GICD_ICFGR0-1 are RAZ/WI so handled separately */
	MPASS(n > 1);
	write_config(hypctx, n, wval);
}

/* GICD_IROUTER */
static void
dist_irouter_read(struct hypctx *hypctx, u_int reg, uint64_t *rval, void *arg)
{
	int n;

	n = (reg - GICD_IROUTER(0)) / 8;
	/* GICD_IROUTER0-31 don't exist */
	MPASS(n > 31);
	*rval = read_route(hypctx, n);
}

static void
dist_irouter_write(struct hypctx *hypctx, u_int reg, u_int offset, u_int size,
    uint64_t wval, void *arg)
{
	int n;

	n = (reg - GICD_IROUTER(0)) / 8;
	/* GICD_IROUTER0-31 don't exist */
	MPASS(n > 31);
	write_route(hypctx, n, wval, offset, size);
}

static bool
vgic_register_read(struct hypctx *hypctx, struct vgic_register *reg_list,
    u_int reg_list_size, u_int reg, u_int size, uint64_t *rval, void *arg)
{
	u_int i, offset;

	for (i = 0; i < reg_list_size; i++) {
		if (reg_list[i].start <= reg && reg_list[i].end >= reg + size) {
			offset = reg & (reg_list[i].size - 1);
			reg -= offset;
			if ((reg_list[i].flags & size) != 0) {
				reg_list[i].read(hypctx, reg, rval, NULL);

				/* Move the bits into the correct place */
				*rval >>= (offset * 8);
				if (size < 8) {
					*rval &= (1ul << (size * 8)) - 1;
				}
			} else {
				/*
				 * The access is an invalid size. Section
				 * 12.1.3 "GIC memory-mapped register access"
				 * of the GICv3 and GICv4 spec issue H
				 * (IHI0069) lists the options. For a read
				 * the controller returns unknown data, in
				 * this case it is zero.
				 */
				*rval = 0;
			}
			return (true);
		}
	}
	return (false);
}

static bool
vgic_register_write(struct hypctx *hypctx, struct vgic_register *reg_list,
    u_int reg_list_size, u_int reg, u_int size, uint64_t wval, void *arg)
{
	u_int i, offset;

	for (i = 0; i < reg_list_size; i++) {
		if (reg_list[i].start <= reg && reg_list[i].end >= reg + size) {
			offset = reg & (reg_list[i].size - 1);
			reg -= offset;
			if ((reg_list[i].flags & size) != 0) {
				reg_list[i].write(hypctx, reg, offset,
				    size, wval, NULL);
			} else {
				/*
				 * See the comment in vgic_register_read.
				 * For writes the controller ignores the
				 * operation.
				 */
			}
			return (true);
		}
	}
	return (false);
}

static int
dist_read(struct vcpu *vcpu, uint64_t fault_ipa, uint64_t *rval,
    int size, void *arg)
{
	struct hyp *hyp;
	struct hypctx *hypctx;
	struct vgic_v3 *vgic;
	uint64_t reg;

	hypctx = vcpu_get_cookie(vcpu);
	hyp = hypctx->hyp;
	vgic = hyp->vgic;

	/* Check the register is one of ours and is the correct size */
	if (fault_ipa < vgic->dist_start || fault_ipa + size > vgic->dist_end) {
		return (EINVAL);
	}

	reg = fault_ipa - vgic->dist_start;
	/*
	 * As described in vgic_register_read an access with an invalid
	 * alignment is read with an unknown value
	 */
	if ((reg & (size - 1)) != 0) {
		*rval = 0;
		return (0);
	}

	if (vgic_register_read(hypctx, dist_registers, nitems(dist_registers),
	    reg, size, rval, NULL))
		return (0);

	/* Reserved register addresses are RES0 so we can hardware it to 0 */
	*rval = 0;

	return (0);
}

static int
dist_write(struct vcpu *vcpu, uint64_t fault_ipa, uint64_t wval,
    int size, void *arg)
{
	struct hyp *hyp;
	struct hypctx *hypctx;
	struct vgic_v3 *vgic;
	uint64_t reg;

	hypctx = vcpu_get_cookie(vcpu);
	hyp = hypctx->hyp;
	vgic = hyp->vgic;

	/* Check the register is one of ours and is the correct size */
	if (fault_ipa < vgic->dist_start || fault_ipa + size > vgic->dist_end) {
		return (EINVAL);
	}

	reg = fault_ipa - vgic->dist_start;
	/*
	 * As described in vgic_register_read an access with an invalid
	 * alignment is write ignored.
	 */
	if ((reg & (size - 1)) != 0)
		return (0);

	if (vgic_register_write(hypctx, dist_registers, nitems(dist_registers),
	    reg, size, wval, NULL))
		return (0);

	/* Reserved register addresses are RES0 so we can ignore the write */
	return (0);
}

/*
 * Redistributor register handlers.
 *
 * RD_base:
 */
/* GICR_CTLR */
static void
redist_ctlr_read(struct hypctx *hypctx, u_int reg, uint64_t *rval, void *arg)
{
	/* LPIs not supported */
	*rval = 0;
}

/* GICR_IIDR */
static void
redist_iidr_read(struct hypctx *hypctx, u_int reg, uint64_t *rval, void *arg)
{
	*rval = VGIC_IIDR;
}

/* GICR_TYPER */
static void
redist_typer_read(struct hypctx *hypctx, u_int reg, uint64_t *rval, void *arg)
{
	uint64_t aff, gicr_typer, vmpidr_el2;
	bool last_vcpu;

	last_vcpu = false;
	if (vcpu_vcpuid(hypctx->vcpu) == (vgic_max_cpu_count(hypctx->hyp) - 1))
		last_vcpu = true;

	vmpidr_el2 = hypctx->vmpidr_el2;
	MPASS(vmpidr_el2 != 0);
	/*
	 * Get affinity for the current CPU. The guest CPU affinity is taken
	 * from VMPIDR_EL2. The Redistributor corresponding to this CPU is
	 * the Redistributor with the same affinity from GICR_TYPER.
	 */
	aff = (CPU_AFF3(vmpidr_el2) << 24) | (CPU_AFF2(vmpidr_el2) << 16) |
	    (CPU_AFF1(vmpidr_el2) << 8) | CPU_AFF0(vmpidr_el2);

	/* Set up GICR_TYPER. */
	gicr_typer = aff << GICR_TYPER_AFF_SHIFT;
	/* Set the vcpu as the processsor ID */
	gicr_typer |=
	    (uint64_t)vcpu_vcpuid(hypctx->vcpu) << GICR_TYPER_CPUNUM_SHIFT;

	if (last_vcpu)
		/* Mark the last Redistributor */
		gicr_typer |= GICR_TYPER_LAST;

	*rval = gicr_typer;
}

/*
 * SGI_base:
 */
/* GICR_ISENABLER0 */
static void
redist_ienabler0_read(struct hypctx *hypctx, u_int reg, uint64_t *rval,
    void *arg)
{
	*rval = read_enabler(hypctx, 0);
}

static void
redist_isenabler0_write(struct hypctx *hypctx, u_int reg, u_int offset,
    u_int size, uint64_t wval, void *arg)
{
	MPASS(offset == 0);
	MPASS(size == 4);
	write_enabler(hypctx, 0, true, wval);
}

/* GICR_ICENABLER0 */
static void
redist_icenabler0_write(struct hypctx *hypctx, u_int reg, u_int offset,
    u_int size, uint64_t wval, void *arg)
{
	MPASS(offset == 0);
	MPASS(size == 4);
	write_enabler(hypctx, 0, false, wval);
}

/* GICR_ISPENDR0 */
static void
redist_ipendr0_read(struct hypctx *hypctx, u_int reg, uint64_t *rval,
    void *arg)
{
	*rval = read_pendr(hypctx, 0);
}

static void
redist_ispendr0_write(struct hypctx *hypctx, u_int reg, u_int offset,
    u_int size, uint64_t wval, void *arg)
{
	MPASS(offset == 0);
	MPASS(size == 4);
	write_pendr(hypctx, 0, true, wval);
}

/* GICR_ICPENDR0 */
static void
redist_icpendr0_write(struct hypctx *hypctx, u_int reg, u_int offset,
    u_int size, uint64_t wval, void *arg)
{
	MPASS(offset == 0);
	MPASS(size == 4);
	write_pendr(hypctx, 0, false, wval);
}

/* GICR_ISACTIVER0 */
static void
redist_iactiver0_read(struct hypctx *hypctx, u_int reg, uint64_t *rval,
    void *arg)
{
	*rval = read_activer(hypctx, 0);
}

static void
redist_isactiver0_write(struct hypctx *hypctx, u_int reg, u_int offset,
    u_int size, uint64_t wval, void *arg)
{
	write_activer(hypctx, 0, true, wval);
}

/* GICR_ICACTIVER0 */
static void
redist_icactiver0_write(struct hypctx *hypctx, u_int reg, u_int offset,
    u_int size, uint64_t wval, void *arg)
{
	write_activer(hypctx, 0, false, wval);
}

/* GICR_IPRIORITYR */
static void
redist_ipriorityr_read(struct hypctx *hypctx, u_int reg, uint64_t *rval,
    void *arg)
{
	int n;

	n = (reg - GICR_IPRIORITYR(0)) / 4;
	*rval = read_priorityr(hypctx, n);
}

static void
redist_ipriorityr_write(struct hypctx *hypctx, u_int reg, u_int offset,
    u_int size, uint64_t wval, void *arg)
{
	u_int irq_base;

	irq_base = (reg - GICR_IPRIORITYR(0)) + offset;
	write_priorityr(hypctx, irq_base, size, wval);
}

/* GICR_ICFGR1 */
static void
redist_icfgr1_read(struct hypctx *hypctx, u_int reg, uint64_t *rval, void *arg)
{
	*rval = read_config(hypctx, 1);
}

static void
redist_icfgr1_write(struct hypctx *hypctx, u_int reg, u_int offset, u_int size,
    uint64_t wval, void *arg)
{
	MPASS(offset == 0);
	MPASS(size == 4);
	write_config(hypctx, 1, wval);
}

static int
redist_read(struct vcpu *vcpu, uint64_t fault_ipa, uint64_t *rval,
    int size, void *arg)
{
	struct hyp *hyp;
	struct hypctx *hypctx, *target_hypctx;
	struct vgic_v3 *vgic;
	uint64_t reg;
	int vcpuid;

	/* Find the current vcpu ctx to get the vgic struct */
	hypctx = vcpu_get_cookie(vcpu);
	hyp = hypctx->hyp;
	vgic = hyp->vgic;

	/* Check the register is one of ours and is the correct size */
	if (fault_ipa < vgic->redist_start ||
	    fault_ipa + size > vgic->redist_end) {
		return (EINVAL);
	}

	vcpuid = (fault_ipa - vgic->redist_start) /
	    (GICR_RD_BASE_SIZE + GICR_SGI_BASE_SIZE);
	if (vcpuid >= vm_get_maxcpus(hyp->vm)) {
		/*
		 * This should never happen, but lets be defensive so if it
		 * does we don't panic a non-INVARIANTS kernel.
		 */
#ifdef INVARIANTS
		panic("%s: Invalid vcpuid %d", __func__, vcpuid);
#else
		*rval = 0;
		return (0);
#endif
	}

	/* Find the target vcpu ctx for the access */
	target_hypctx = hyp->ctx[vcpuid];
	if (target_hypctx == NULL) {
		/*
		 * The CPU has not yet started. The redistributor and CPU are
		 * in the same power domain. As such the redistributor will
		 * also be powered down so any access will raise an external
		 * abort.
		 */
		raise_data_insn_abort(hypctx, fault_ipa, true,
		    ISS_DATA_DFSC_EXT);
		return (0);
	}

	reg = (fault_ipa - vgic->redist_start) %
	    (GICR_RD_BASE_SIZE + GICR_SGI_BASE_SIZE);

	/*
	 * As described in vgic_register_read an access with an invalid
	 * alignment is read with an unknown value
	 */
	if ((reg & (size - 1)) != 0) {
		*rval = 0;
		return (0);
	}

	if (reg < GICR_RD_BASE_SIZE) {
		if (vgic_register_read(target_hypctx, redist_rd_registers,
		    nitems(redist_rd_registers), reg, size, rval, NULL))
			return (0);
	} else if (reg < (GICR_SGI_BASE + GICR_SGI_BASE_SIZE)) {
		if (vgic_register_read(target_hypctx, redist_sgi_registers,
		    nitems(redist_sgi_registers), reg - GICR_SGI_BASE, size,
		    rval, NULL))
			return (0);
	}

	/* Reserved register addresses are RES0 so we can hardware it to 0 */
	*rval = 0;
	return (0);
}

static int
redist_write(struct vcpu *vcpu, uint64_t fault_ipa, uint64_t wval,
    int size, void *arg)
{
	struct hyp *hyp;
	struct hypctx *hypctx, *target_hypctx;
	struct vgic_v3 *vgic;
	uint64_t reg;
	int vcpuid;

	/* Find the current vcpu ctx to get the vgic struct */
	hypctx = vcpu_get_cookie(vcpu);
	hyp = hypctx->hyp;
	vgic = hyp->vgic;

	/* Check the register is one of ours and is the correct size */
	if (fault_ipa < vgic->redist_start ||
	    fault_ipa + size > vgic->redist_end) {
		return (EINVAL);
	}

	vcpuid = (fault_ipa - vgic->redist_start) /
	    (GICR_RD_BASE_SIZE + GICR_SGI_BASE_SIZE);
	if (vcpuid >= vm_get_maxcpus(hyp->vm)) {
		/*
		 * This should never happen, but lets be defensive so if it
		 * does we don't panic a non-INVARIANTS kernel.
		 */
#ifdef INVARIANTS
		panic("%s: Invalid vcpuid %d", __func__, vcpuid);
#else
		return (0);
#endif
	}

	/* Find the target vcpu ctx for the access */
	target_hypctx = hyp->ctx[vcpuid];
	if (target_hypctx == NULL) {
		/*
		 * The CPU has not yet started. The redistributor and CPU are
		 * in the same power domain. As such the redistributor will
		 * also be powered down so any access will raise an external
		 * abort.
		 */
		raise_data_insn_abort(hypctx, fault_ipa, true,
		    ISS_DATA_DFSC_EXT);
		return (0);
	}

	reg = (fault_ipa - vgic->redist_start) %
	    (GICR_RD_BASE_SIZE + GICR_SGI_BASE_SIZE);

	/*
	 * As described in vgic_register_read an access with an invalid
	 * alignment is write ignored.
	 */
	if ((reg & (size - 1)) != 0)
		return (0);

	if (reg < GICR_RD_BASE_SIZE) {
		if (vgic_register_write(target_hypctx, redist_rd_registers,
		    nitems(redist_rd_registers), reg, size, wval, NULL))
			return (0);
	} else if (reg < (GICR_SGI_BASE + GICR_SGI_BASE_SIZE)) {
		if (vgic_register_write(target_hypctx, redist_sgi_registers,
		    nitems(redist_sgi_registers), reg - GICR_SGI_BASE, size,
		    wval, NULL))
			return (0);
	}

	/* Reserved register addresses are RES0 so we can ignore the write */
	return (0);
}

static int
vgic_v3_icc_sgi1r_read(struct vcpu *vcpu, uint64_t *rval, void *arg)
{
	/*
	 * TODO: Inject an unknown exception.
	 */
	*rval = 0;
	return (0);
}

static int
vgic_v3_icc_sgi1r_write(struct vcpu *vcpu, uint64_t rval, void *arg)
{
	struct vm *vm;
	struct hyp *hyp;
	cpuset_t active_cpus;
	uint64_t mpidr, aff1, aff2, aff3;
	uint32_t irqid;
	int cpus, cpu_off, target_vcpuid, vcpuid;

	vm = vcpu_vm(vcpu);
	hyp = vm_get_cookie(vm);
	active_cpus = vm_active_cpus(vm);
	vcpuid = vcpu_vcpuid(vcpu);

	irqid = ICC_SGI1R_EL1_SGIID_VAL(rval) >> ICC_SGI1R_EL1_SGIID_SHIFT;
	if ((rval & ICC_SGI1R_EL1_IRM) == 0) {
		/* Non-zero points at no vcpus */
		if (ICC_SGI1R_EL1_RS_VAL(rval) != 0)
			return (0);

		aff1 = ICC_SGI1R_EL1_AFF1_VAL(rval) >> ICC_SGI1R_EL1_AFF1_SHIFT;
		aff2 = ICC_SGI1R_EL1_AFF2_VAL(rval) >> ICC_SGI1R_EL1_AFF2_SHIFT;
		aff3 = ICC_SGI1R_EL1_AFF3_VAL(rval) >> ICC_SGI1R_EL1_AFF3_SHIFT;
		mpidr = aff3 << MPIDR_AFF3_SHIFT |
		    aff2 << MPIDR_AFF2_SHIFT | aff1 << MPIDR_AFF1_SHIFT;

		cpus = ICC_SGI1R_EL1_TL_VAL(rval) >> ICC_SGI1R_EL1_TL_SHIFT;
		cpu_off = 0;
		while (cpus > 0) {
			if (cpus & 1) {
				target_vcpuid = mpidr_to_vcpu(hyp,
				    mpidr | (cpu_off << MPIDR_AFF0_SHIFT));
				if (target_vcpuid >= 0 &&
				    CPU_ISSET(target_vcpuid, &active_cpus)) {
					INJECT_IRQ(hyp, target_vcpuid, irqid,
					    true);
				}
			}
			cpu_off++;
			cpus >>= 1;
		}
	} else {
		/* Send an IPI to all CPUs other than the current CPU */
		for (target_vcpuid = 0; target_vcpuid < vm_get_maxcpus(vm);
		    target_vcpuid++) {
			if (CPU_ISSET(target_vcpuid, &active_cpus) &&
			    target_vcpuid != vcpuid) {
				INJECT_IRQ(hyp, target_vcpuid, irqid, true);
			}
		}
	}

	return (0);
}

static void
vgic_v3_mmio_init(struct hyp *hyp)
{
	struct vgic_v3 *vgic;
	struct vgic_v3_irq *irq;
	int i;

	/* Allocate memory for the SPIs */
	vgic = hyp->vgic;
	vgic->irqs = malloc((VGIC_NIRQS - VGIC_PRV_I_NUM) *
	    sizeof(*vgic->irqs), M_VGIC_V3, M_WAITOK | M_ZERO);

	for (i = 0; i < VGIC_NIRQS - VGIC_PRV_I_NUM; i++) {
		irq = &vgic->irqs[i];

		mtx_init(&irq->irq_spinmtx, "VGIC IRQ spinlock", NULL,
		    MTX_SPIN);

		irq->irq = i + VGIC_PRV_I_NUM;
	}
}

static void
vgic_v3_mmio_destroy(struct hyp *hyp)
{
	struct vgic_v3 *vgic;
	struct vgic_v3_irq *irq;
	int i;

	vgic = hyp->vgic;
	for (i = 0; i < VGIC_NIRQS - VGIC_PRV_I_NUM; i++) {
		irq = &vgic->irqs[i];

		mtx_destroy(&irq->irq_spinmtx);
	}

	free(vgic->irqs, M_VGIC_V3);
}

static int
vgic_v3_attach_to_vm(device_t dev, struct hyp *hyp, struct vm_vgic_descr *descr)
{
	struct vm *vm;
	struct vgic_v3 *vgic;
	size_t cpu_count;

	if (descr->ver.version != 3)
		return (EINVAL);

	/*
	 * The register bases need to be 64k aligned
	 * The redist register space is the RD + SGI size
	 */
	if (!__is_aligned(descr->v3_regs.dist_start, PAGE_SIZE_64K) ||
	    !__is_aligned(descr->v3_regs.redist_start, PAGE_SIZE_64K) ||
	    !__is_aligned(descr->v3_regs.redist_size,
	     GICR_RD_BASE_SIZE + GICR_SGI_BASE_SIZE))
		return (EINVAL);

	/* The dist register space is 1 64k block */
	if (descr->v3_regs.dist_size != PAGE_SIZE_64K)
		return (EINVAL);

	vm = hyp->vm;

	/*
	 * Return an error if the redist space is too large for the maximum
	 * number of CPUs we support.
	 */
	cpu_count = descr->v3_regs.redist_size /
	    (GICR_RD_BASE_SIZE + GICR_SGI_BASE_SIZE);
	if (cpu_count > vm_get_maxcpus(vm))
		return (EINVAL);

	vgic = hyp->vgic;

	/* Set the distributor address and size for trapping guest access. */
	vgic->dist_start = descr->v3_regs.dist_start;
	vgic->dist_end = descr->v3_regs.dist_start + descr->v3_regs.dist_size;

	vgic->redist_start = descr->v3_regs.redist_start;
	vgic->redist_end = descr->v3_regs.redist_start +
	    descr->v3_regs.redist_size;

	vm_register_inst_handler(vm, descr->v3_regs.dist_start,
	    descr->v3_regs.dist_size, dist_read, dist_write);
	vm_register_inst_handler(vm, descr->v3_regs.redist_start,
	    descr->v3_regs.redist_size, redist_read, redist_write);

	vm_register_reg_handler(vm, ISS_MSR_REG(ICC_SGI1R_EL1),
	    ISS_MSR_REG_MASK, vgic_v3_icc_sgi1r_read, vgic_v3_icc_sgi1r_write,
	    NULL);

	vgic_v3_mmio_init(hyp);

	hyp->vgic_attached = true;

	return (0);
}

static void
vgic_v3_detach_from_vm(device_t dev, struct hyp *hyp)
{
	if (hyp->vgic_attached) {
		hyp->vgic_attached = false;
		vgic_v3_mmio_destroy(hyp);
	}
}

static struct vgic_v3_irq *
vgic_v3_get_irq(struct hyp *hyp, int vcpuid, uint32_t irqid)
{
	struct vgic_v3_cpu *vgic_cpu;
	struct vgic_v3_irq *irq;
	struct hypctx *hypctx;

	if (irqid < VGIC_PRV_I_NUM) {
		if (vcpuid < 0 || vcpuid >= vm_get_maxcpus(hyp->vm))
			return (NULL);
		hypctx = hyp->ctx[vcpuid];
		if (hypctx == NULL)
			return (NULL);
		vgic_cpu = hypctx->vgic_cpu;
		irq = &vgic_cpu->private_irqs[irqid];
	} else if (irqid <= GIC_LAST_SPI) {
		irqid -= VGIC_PRV_I_NUM;
		if (irqid >= VGIC_NIRQS)
			return (NULL);
		irq = &hyp->vgic->irqs[irqid];
	} else if (irqid < GIC_FIRST_LPI) {
		return (NULL);
	} else {
		/* No support for LPIs */
		return (NULL);
	}

	mtx_lock_spin(&irq->irq_spinmtx);
	return (irq);
}

static void
vgic_v3_release_irq(struct vgic_v3_irq *irq)
{

	mtx_unlock_spin(&irq->irq_spinmtx);
}

static bool
vgic_v3_has_pending_irq(device_t dev, struct hypctx *hypctx)
{
	struct vgic_v3_cpu *vgic_cpu;
	bool empty;

	vgic_cpu = hypctx->vgic_cpu;
	mtx_lock_spin(&vgic_cpu->lr_mtx);
	empty = TAILQ_EMPTY(&vgic_cpu->irq_act_pend);
	mtx_unlock_spin(&vgic_cpu->lr_mtx);

	return (!empty);
}

static bool
vgic_v3_check_irq(struct vgic_v3_irq *irq, bool level)
{
	/*
	 * Only inject if:
	 *  - Level-triggered IRQ: level changes low -> high
	 *  - Edge-triggered IRQ: level is high
	 */
	switch (irq->config & VGIC_CONFIG_MASK) {
	case VGIC_CONFIG_LEVEL:
		return (level != irq->level);
	case VGIC_CONFIG_EDGE:
		return (level);
	default:
		break;
	}

	return (false);
}

static int
vgic_v3_inject_irq(device_t dev, struct hyp *hyp, int vcpuid, uint32_t irqid,
    bool level)
{
	struct vgic_v3_cpu *vgic_cpu;
	struct vgic_v3_irq *irq;
	struct hypctx *hypctx;
	int target_vcpu;
	bool notify;

	if (!hyp->vgic_attached)
		return (ENODEV);

	KASSERT(vcpuid == -1 || irqid < VGIC_PRV_I_NUM,
	    ("%s: SPI/LPI with vcpuid set: irq %u vcpuid %u", __func__, irqid,
	    vcpuid));

	irq = vgic_v3_get_irq(hyp, vcpuid, irqid);
	if (irq == NULL) {
		eprintf("Malformed IRQ %u.\n", irqid);
		return (EINVAL);
	}

	target_vcpu = irq->target_vcpu;
	KASSERT(vcpuid == -1 || vcpuid == target_vcpu,
	    ("%s: Interrupt %u has bad cpu affinity: vcpu %d target vcpu %d",
	    __func__, irqid, vcpuid, target_vcpu));
	KASSERT(target_vcpu >= 0 && target_vcpu < vm_get_maxcpus(hyp->vm),
	    ("%s: Interrupt %u sent to invalid vcpu %d", __func__, irqid,
	    target_vcpu));

	if (vcpuid == -1)
		vcpuid = target_vcpu;
	/* TODO: Check from 0 to vm->maxcpus */
	if (vcpuid < 0 || vcpuid >= vm_get_maxcpus(hyp->vm)) {
		vgic_v3_release_irq(irq);
		return (EINVAL);
	}

	hypctx = hyp->ctx[vcpuid];
	if (hypctx == NULL) {
		vgic_v3_release_irq(irq);
		return (EINVAL);
	}

	notify = false;
	vgic_cpu = hypctx->vgic_cpu;

	mtx_lock_spin(&vgic_cpu->lr_mtx);

	if (!vgic_v3_check_irq(irq, level)) {
		goto out;
	}

	if ((irq->config & VGIC_CONFIG_MASK) == VGIC_CONFIG_LEVEL)
		irq->level = level;
	else /* VGIC_CONFIG_EDGE */
		irq->pending = true;

	notify = vgic_v3_queue_irq(hyp, vgic_cpu, vcpuid, irq);

out:
	mtx_unlock_spin(&vgic_cpu->lr_mtx);
	vgic_v3_release_irq(irq);

	if (notify)
		vcpu_notify_event(vm_vcpu(hyp->vm, vcpuid));

	return (0);
}

static int
vgic_v3_inject_msi(device_t dev, struct hyp *hyp, uint64_t msg, uint64_t addr)
{
	struct vgic_v3 *vgic;
	uint64_t reg;

	vgic = hyp->vgic;

	/* This is a 4 byte register */
	if (addr < vgic->dist_start || addr + 4 > vgic->dist_end) {
		return (EINVAL);
	}

	reg = addr - vgic->dist_start;
	if (reg != GICD_SETSPI_NSR)
		return (EINVAL);

	return (INJECT_IRQ(hyp, -1, msg, true));
}

static void
vgic_v3_flush_hwstate(device_t dev, struct hypctx *hypctx)
{
	struct vgic_v3_cpu *vgic_cpu;
	struct vgic_v3_irq *irq;
	int i;

	vgic_cpu = hypctx->vgic_cpu;

	/*
	 * All Distributor writes have been executed at this point, do not
	 * protect Distributor reads with a mutex.
	 *
	 * This is callled with all interrupts disabled, so there is no need for
	 * a List Register spinlock either.
	 */
	mtx_lock_spin(&vgic_cpu->lr_mtx);

	hypctx->vgic_v3_regs.ich_hcr_el2 &= ~ICH_HCR_EL2_UIE;

	/* Exit early if there are no buffered interrupts */
	if (TAILQ_EMPTY(&vgic_cpu->irq_act_pend))
		goto out;

	KASSERT(vgic_cpu->ich_lr_used == 0, ("%s: Used LR count not zero %u",
	    __func__, vgic_cpu->ich_lr_used));

	i = 0;
	hypctx->vgic_v3_regs.ich_elrsr_el2 =
	    (1u << hypctx->vgic_v3_regs.ich_lr_num) - 1;
	TAILQ_FOREACH(irq, &vgic_cpu->irq_act_pend, act_pend_list) {
		/* No free list register, stop searching for IRQs */
		if (i == hypctx->vgic_v3_regs.ich_lr_num)
			break;

		if (!irq->enabled)
			continue;

		hypctx->vgic_v3_regs.ich_lr_el2[i] = ICH_LR_EL2_GROUP1 |
		    ((uint64_t)irq->priority << ICH_LR_EL2_PRIO_SHIFT) |
		    irq->irq;

		if (irq->active) {
			hypctx->vgic_v3_regs.ich_lr_el2[i] |=
			    ICH_LR_EL2_STATE_ACTIVE;
		}

#ifdef notyet
		/* TODO: Check why this is needed */
		if ((irq->config & _MASK) == LEVEL)
			hypctx->vgic_v3_regs.ich_lr_el2[i] |= ICH_LR_EL2_EOI;
#endif

		if (!irq->active && vgic_v3_irq_pending(irq)) {
			hypctx->vgic_v3_regs.ich_lr_el2[i] |=
			    ICH_LR_EL2_STATE_PENDING;

			/*
			 * This IRQ is now pending on the guest. Allow for
			 * another edge that could cause the interrupt to
			 * be raised again.
			 */
			if ((irq->config & VGIC_CONFIG_MASK) ==
			    VGIC_CONFIG_EDGE) {
				irq->pending = false;
			}
		}

		i++;
	}
	vgic_cpu->ich_lr_used = i;

out:
	mtx_unlock_spin(&vgic_cpu->lr_mtx);
}

static void
vgic_v3_sync_hwstate(device_t dev, struct hypctx *hypctx)
{
	struct vgic_v3_cpu *vgic_cpu;
	struct vgic_v3_irq *irq;
	uint64_t lr;
	int i;

	vgic_cpu = hypctx->vgic_cpu;

	/* Exit early if there are no buffered interrupts */
	if (vgic_cpu->ich_lr_used == 0)
		return;

	/*
	 * Check on the IRQ state after running the guest. ich_lr_used and
	 * ich_lr_el2 are only ever used within this thread so is safe to
	 * access unlocked.
	 */
	for (i = 0; i < vgic_cpu->ich_lr_used; i++) {
		lr = hypctx->vgic_v3_regs.ich_lr_el2[i];
		hypctx->vgic_v3_regs.ich_lr_el2[i] = 0;

		irq = vgic_v3_get_irq(hypctx->hyp, vcpu_vcpuid(hypctx->vcpu),
		    ICH_LR_EL2_VINTID(lr));
		if (irq == NULL)
			continue;

		irq->active = (lr & ICH_LR_EL2_STATE_ACTIVE) != 0;

		if ((irq->config & VGIC_CONFIG_MASK) == VGIC_CONFIG_EDGE) {
			/*
			 * If we have an edge triggered IRQ preserve the
			 * pending bit until the IRQ has been handled.
			 */
			if ((lr & ICH_LR_EL2_STATE_PENDING) != 0) {
				irq->pending = true;
			}
		} else {
			/*
			 * If we have a level triggerend IRQ remove the
			 * pending bit if the IRQ has been handled.
			 * The level is separate, so may still be high
			 * triggering another IRQ.
			 */
			if ((lr & ICH_LR_EL2_STATE_PENDING) == 0) {
				irq->pending = false;
			}
		}

		/* Lock to update irq_act_pend */
		mtx_lock_spin(&vgic_cpu->lr_mtx);
		if (irq->active) {
			/* Ensure the active IRQ is at the head of the list */
			TAILQ_REMOVE(&vgic_cpu->irq_act_pend, irq,
			    act_pend_list);
			TAILQ_INSERT_HEAD(&vgic_cpu->irq_act_pend, irq,
			    act_pend_list);
		} else if (!vgic_v3_irq_pending(irq)) {
			/* If pending or active remove from the list */
			TAILQ_REMOVE(&vgic_cpu->irq_act_pend, irq,
			    act_pend_list);
			irq->on_aplist = false;
		}
		mtx_unlock_spin(&vgic_cpu->lr_mtx);
		vgic_v3_release_irq(irq);
	}

	hypctx->vgic_v3_regs.ich_hcr_el2 &= ~ICH_HCR_EL2_EOICOUNT_MASK;
	vgic_cpu->ich_lr_used = 0;
}

static void
vgic_v3_init(device_t dev)
{
	uint64_t ich_vtr_el2;
	uint32_t pribits, prebits;

	ich_vtr_el2 = vmm_read_reg(HYP_REG_ICH_VTR);

	/* TODO: These fields are common with the vgicv2 driver */
	pribits = ICH_VTR_EL2_PRIBITS(ich_vtr_el2);
	switch (pribits) {
	default:
	case 5:
		virt_features.min_prio = 0xf8;
		break;
	case 6:
		virt_features.min_prio = 0xfc;
		break;
	case 7:
		virt_features.min_prio = 0xfe;
		break;
	case 8:
		virt_features.min_prio = 0xff;
		break;
	}

	prebits = ICH_VTR_EL2_PREBITS(ich_vtr_el2);
	switch (prebits) {
	default:
	case 5:
		virt_features.ich_apr_num = 1;
		break;
	case 6:
		virt_features.ich_apr_num = 2;
		break;
	case 7:
		virt_features.ich_apr_num = 4;
		break;
	}

	virt_features.ich_lr_num = ICH_VTR_EL2_LISTREGS(ich_vtr_el2);
}

static int
vgic_v3_probe(device_t dev)
{
	if (!gic_get_vgic(dev))
		return (EINVAL);

	/* We currently only support the GICv3 */
	if (gic_get_hw_rev(dev) < 3)
		return (EINVAL);

	device_set_desc(dev, "Virtual GIC v3");
	return (BUS_PROBE_DEFAULT);
}

static int
vgic_v3_attach(device_t dev)
{
	vgic_dev = dev;
	return (0);
}

static int
vgic_v3_detach(device_t dev)
{
	vgic_dev = NULL;
	return (0);
}

static device_method_t vgic_v3_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		vgic_v3_probe),
	DEVMETHOD(device_attach,	vgic_v3_attach),
	DEVMETHOD(device_detach,	vgic_v3_detach),

	/* VGIC interface */
	DEVMETHOD(vgic_init,		vgic_v3_init),
	DEVMETHOD(vgic_attach_to_vm,	vgic_v3_attach_to_vm),
	DEVMETHOD(vgic_detach_from_vm,	vgic_v3_detach_from_vm),
	DEVMETHOD(vgic_vminit,		vgic_v3_vminit),
	DEVMETHOD(vgic_cpuinit,		vgic_v3_cpuinit),
	DEVMETHOD(vgic_cpucleanup,	vgic_v3_cpucleanup),
	DEVMETHOD(vgic_vmcleanup,	vgic_v3_vmcleanup),
	DEVMETHOD(vgic_max_cpu_count,	vgic_v3_max_cpu_count),
	DEVMETHOD(vgic_has_pending_irq,	vgic_v3_has_pending_irq),
	DEVMETHOD(vgic_inject_irq,	vgic_v3_inject_irq),
	DEVMETHOD(vgic_inject_msi,	vgic_v3_inject_msi),
	DEVMETHOD(vgic_flush_hwstate,	vgic_v3_flush_hwstate),
	DEVMETHOD(vgic_sync_hwstate,	vgic_v3_sync_hwstate),

	/* End */
	DEVMETHOD_END
};

/* TODO: Create a vgic base class? */
DEFINE_CLASS_0(vgic, vgic_v3_driver, vgic_v3_methods, 0);

DRIVER_MODULE(vgic_v3, gic, vgic_v3_driver, 0, 0);
