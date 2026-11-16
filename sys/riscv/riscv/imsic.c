/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Ruslan Bukin <br@bsdpad.com>
 * Copyright (c) 2024 Himanshu Chauhan <hchauhan@thechauhan.dev>
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

#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/rman.h>
#include <sys/smp.h>
#include <sys/mutex.h>
#include <sys/intr.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/imsic.h>
#include <machine/smp.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "pic_if.h"
#include "msi_if.h"

#define	IMSIC_MSI_PPN_SHIFT	12	/* Number of bits to shift for PPN. */
#define	IMSIC_PAGE_SHIFT	12	/* Interrupt file page shift. */

#define	IMSIC_EIDELIVERY	0x70
#define	 EIDELIVERY_EN		(1 << 0)
#define	IMSIC_EITHRESHOLD	0x72
#define	 ITHRESHOLD_DIS		0
#define	IMSIC_EIP(n)		(0x80 + ((n / 64) * 2))
#define	IMSIC_EIE(n)		(0xC0 + ((n / 64) * 2))

#define	IMSIC_MAX_MSI		2048	/* Max MSI ID per core (per spec). */
BITSET_DEFINE(msiset, IMSIC_MAX_MSI);
#define	IMSIC_FILL(p)		BIT_FILL(IMSIC_MAX_MSI, p)
#define	IMSIC_ISEMPTY(p)	BIT_EMPTY(IMSIC_MAX_MSI, p)
#define	IMSIC_FFS(p)		BIT_FFS(IMSIC_MAX_MSI, p)
#define	IMSIC_SET(n, p)		BIT_SET(IMSIC_MAX_MSI, n, p)
#define	IMSIC_CLR(n, p)		BIT_CLR(IMSIC_MAX_MSI, n, p)
#define	IMSIC_LOCK(_sc, _cpu)	mtx_lock_spin(&(_sc)->pcpu_data[_cpu].lock)
#define	IMSIC_UNLOCK(_sc, _cpu)	mtx_unlock_spin(&(_sc)->pcpu_data[_cpu].lock)

#define ISRC_TO_IRQ(_isrc)	((struct imsic_irq *)(_isrc))

struct imsic_msi_config {
	u_int lhxs;		/* Low hart index shift */
	u_int lhxw;		/* Low hart index width */
	u_int hhxs;		/* High hart index shift */
	u_int hhxw;		/* High hart index width */
	vm_paddr_t msi_base;	/* Base address for PPN */
	unsigned long msi_sz;	/* Total MSI region size for all harts */
};

#ifdef SMP
struct smp_action {
	int irq;
};
#endif

struct imsic_irq {
	struct intr_irqsrc isrc;
	int cpuid;
	int msi;
};

struct imsic_pcpu {
	struct mtx lock;
	struct msiset allocated_msis;
	struct imsic_irq *irqs[IMSIC_MAX_MSI];
	uint16_t aplic_irq[IMSIC_MAX_MSI];
};

struct imsic_softc {
	device_t dev;
	struct intr_pic *pic;
	struct resource *mem_res;
	struct resource *irq_res;
	struct imsic_pcpu *pcpu_data;
	struct imsic_msi_config msi_config;
	cpuset_t target_cpus;
	int nr_parent_irqs;
	int ndev;
};

static inline void
imsic_csr_write(int reg, uint64_t val)
{

	csr_write(siselect, reg);
	csr_write(sireg, val);
}

static inline uint64_t
imsic_csr_read(int reg)
{
	uint64_t val;

	csr_write(siselect, reg);
	val = csr_read(sireg);

	return (val);
}

static inline uint32_t
riscv_cpu_to_hartid(int cpu)
{

	return (pcpu_find(cpu)->pc_hart);
}

static inline int
riscv_hartid_to_cpu(uint32_t hartid)
{
	int cpu;

	CPU_FOREACH(cpu) {
		if (riscv_cpu_to_hartid(cpu) == hartid)
			return (cpu);
	}

	return (-1);
}

static int
fdt_get_hartid(device_t dev, phandle_t imsic, uint32_t *hartid)
{

	/* Check the interrupt controller layout. */
	if (OF_searchencprop(imsic, "#interrupt-cells", hartid,
	    sizeof(*hartid)) == -1) {
		device_printf(dev,
		    "Could not find #interrupt-cells for phandle %u\n", imsic);
		return (-1);
	}

	/*
	 * The parent of the interrupt-controller is the CPU we are
	 * interested in, so search for its hart ID.
	 */
	if (OF_searchencprop(OF_parent(imsic), "reg", (pcell_t *)hartid,
	    sizeof(*hartid)) == -1) {
		device_printf(dev, "Could not find hartid\n");
		return (-1);
	}

	return (0);
}

static int
imsic_intr(void *arg)
{
	struct imsic_softc *sc;
	struct trapframe *tf;
	struct imsic_irq *irq;
	uint16_t aplic_irq;
	u_long msi;
	int cpu;

	cpu = PCPU_GET(cpuid);

	sc = arg;

	while ((msi = csr_swap(stopei, 0))) {
		msi >>= 16;

		aplic_irq = sc->pcpu_data[cpu].aplic_irq[msi];
		if (aplic_irq) {
			intr_child_irq_handler(sc->pic, aplic_irq);
			continue;
		}

		/* This IRQ is handled by IMSIC. */
		irq = sc->pcpu_data[cpu].irqs[msi];
		tf = curthread->td_intr_frame;
		if (intr_isrc_dispatch(&irq->isrc, tf) != 0) {
			device_printf(sc->dev, "Stray irq %lu detected\n", msi);
			return (FILTER_STRAY);
		}
	}

	return (FILTER_HANDLED);
}

static inline void
imsic_enable_interrupt(int irq)
{
	uint64_t reg;

	reg = imsic_csr_read(IMSIC_EIE(irq));
	reg |= (0x1UL << (irq % 64));
	imsic_csr_write(IMSIC_EIE(irq), reg);
}

static inline void
imsic_disable_interrupt(int irq)
{
	uint64_t reg;

	reg = imsic_csr_read(IMSIC_EIE(irq));
	reg &= ~(0x1UL << (irq % 64));
	imsic_csr_write(IMSIC_EIE(irq), reg);
}

#ifdef SMP
static void
smp_imsic_enable_interrupt(void *arg)
{
	struct smp_action *act;

	act = arg;

	imsic_enable_interrupt(act->irq);
}

static void
smp_imsic_disable_interrupt(void *arg)
{
	struct smp_action *act;

	act = arg;

	imsic_disable_interrupt(act->irq);
}
#endif

static int
imsic_enable_irq(device_t dev, int cpu, int irq)
{
#ifdef SMP
	struct smp_action act;
	cpuset_t cpus;
#endif
	if (cpu == PCPU_GET(cpuid)) {
		imsic_enable_interrupt(irq);
	} else {
#ifdef SMP
		CPU_ZERO(&cpus);
		CPU_SET(cpu, &cpus);
		act.irq = irq;
		smp_rendezvous_cpus(cpus, NULL, smp_imsic_enable_interrupt,
		    NULL, &act);
#endif
	}

	return (0);
}

static int
imsic_disable_irq(device_t dev, int cpu, int irq)
{
#ifdef SMP
	struct smp_action act;
	cpuset_t cpus;
#endif
	if (cpu == PCPU_GET(cpuid)) {
		imsic_disable_interrupt(irq);
	} else {
#ifdef SMP
		CPU_ZERO(&cpus);
		CPU_SET(cpu, &cpus);
		act.irq = irq;
		smp_rendezvous_cpus(cpus, NULL, smp_imsic_disable_interrupt,
		    NULL, &act);
#endif
	}

	return (0);
}

static int
imsic_nr_parent_irqs(device_t dev)
{
	struct imsic_softc *sc;
	phandle_t node;
	pcell_t *cells;
	uint32_t hartid;
	int error;
	int nintr;
	int cpu;
	int rc, i;

	rc = ENXIO;

	sc = device_get_softc(dev);
	node = ofw_bus_get_node(dev);

	sc->nr_parent_irqs = 0;

	nintr = OF_getencprop_alloc_multi(node, "interrupts-extended",
	    sizeof(uint32_t), (void **)&cells);
	if (nintr < 0) {
		device_printf(dev, "interrupts-extended property not found.\n");
		goto out;
	}

	CPU_ZERO(&sc->target_cpus);

	for (i = 0; i < nintr; i += 2) {
		if (cells[i + 1] != IRQ_EXTERNAL_SUPERVISOR)
			continue;

		error = fdt_get_hartid(dev, OF_node_from_xref(cells[i]),
		    &hartid);
		if (error != 0) {
			device_printf(dev, "Target hartid not found\n");
			goto fail;
		}

		cpu = riscv_hartid_to_cpu(hartid);
		if (cpu == -1) {
			device_printf(dev, "Invalid cpu for hart %d\n",
			    hartid);
			continue;
		}

		CPU_SET(cpu, &sc->target_cpus);
		sc->nr_parent_irqs += 1;
	}

	rc = 0;

fail:
	OF_prop_free(cells);
out:
	return (rc);
}

static int
imsic_parse_fdt_msi_config(device_t dev)
{
	struct imsic_softc *sc;
	phandle_t node;
	pcell_t *buf;
	ssize_t len;
	int error;

	sc = device_get_softc(dev);
	node = ofw_bus_get_node(dev);

	if ((len = OF_getencprop_alloc(node, "riscv,guest-index-bits",
	    (void **)&buf)) < 0)
		sc->msi_config.lhxs = 0;
	else
		sc->msi_config.lhxs = buf[0];

	OF_prop_free(buf);

	if ((len = OF_getencprop_alloc(node, "riscv,hart-index-bits",
	    (void *)&buf)) < 0) {
		sc->msi_config.lhxw = fls(sc->nr_parent_irqs);
		if ((1UL << sc->msi_config.lhxw) < sc->nr_parent_irqs)
			sc->msi_config.lhxw++;
	} else
		sc->msi_config.lhxw = buf[0];

	OF_prop_free(buf);

	if ((len = OF_getencprop_alloc(node, "riscv,group-index-bits",
	    (void *)&buf)) < 0)
		sc->msi_config.hhxw = 0;
	else
		sc->msi_config.hhxw = buf[0];

	OF_prop_free(buf);

	if ((len = OF_getencprop_alloc(node, "riscv,group-index-shift",
	    (void *)&buf)) < 0)
		sc->msi_config.hhxs = 2 * IMSIC_PAGE_SHIFT;
	else
		sc->msi_config.hhxs = buf[0];

	OF_prop_free(buf);

	if (sc->msi_config.hhxs < (2 * IMSIC_PAGE_SHIFT))
		return (ENXIO);

	/* Now find the MSI base. */
	error = fdt_regsize(node, &sc->msi_config.msi_base,
	    &sc->msi_config.msi_sz);

	return (error);
}

static int
imsic_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "riscv,imsics"))
		return (ENXIO);

	if (!ofw_bus_has_prop(dev, "msi-controller"))
		return (ENXIO);

	device_set_desc(dev, "Incoming MSI Controller");

	return (BUS_PROBE_DEFAULT);
}

static int
imsic_attach(device_t dev)
{
	struct imsic_softc *sc;
	phandle_t node;
	phandle_t xref;
	int i, irq, error, rid;
	phandle_t iparent;
	pcell_t cell;
	device_t root_dev;
	phandle_t root_node;

	sc = device_get_softc(dev);
	node = ofw_bus_get_node(dev);
	sc->dev = dev;

	sc->pcpu_data = mallocarray(mp_maxid + 1, sizeof(struct imsic_pcpu),
	    M_DEVBUF, M_WAITOK | M_ZERO);

	if (imsic_nr_parent_irqs(dev)) {
		device_printf(dev, "Could not parse DTS.\n");
		return (ENXIO);
	}

	error = imsic_parse_fdt_msi_config(dev);
	if (error) {
		device_printf(dev, "Could not read config.\n");
		return (ENXIO);
	}

	/*
	 * Enable interrupt delivery by interrupt file. APLIC will forward the
	 * interrupts to IMSIC as MSI writes.
	 */
	imsic_csr_write(IMSIC_EIDELIVERY, EIDELIVERY_EN);

	/* Receive all interrupts */
	imsic_csr_write(IMSIC_EITHRESHOLD, ITHRESHOLD_DIS);

	/* Per cpu MSI IDs. */
	CPU_FOREACH(i) {
		mtx_init(&sc->pcpu_data[i].lock, "imsic-pcpu-lock",
		    NULL, MTX_SPIN);
		IMSIC_FILL(&sc->pcpu_data[i].allocated_msis);
		/* MSI ID 0 is not a valid MSI. */
		IMSIC_CLR(0, &sc->pcpu_data[i].allocated_msis);
	}

	xref = OF_xref_from_node(node);
	sc->pic = intr_pic_register(dev, xref);
	if (sc->pic == NULL) {
		device_printf(dev, "PIC registration unsuccessful\n");
		return (ENXIO);
	}
	intr_msi_register(dev, xref);

	/* Find our parent interrupt controller. */
	root_dev = device_get_parent(dev);
	root_node = ofw_bus_get_node(root_dev);
	iparent = OF_xref_from_node(root_node);
	cell = IRQ_EXTERNAL_SUPERVISOR;
	irq = ofw_bus_map_intr(dev, iparent, 1, &cell);
	error = bus_set_resource(dev, SYS_RES_IRQ, 0, irq, 1);
	if (error != 0) {
		device_printf(dev, "Unable to register IRQ resource\n");
		return (ENXIO);
	}

	rid = 0;
	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE | RF_SHAREABLE);
	if (sc->irq_res == NULL) {
		device_printf(dev, "Unable to alloc IRQ resource\n");
		return (ENXIO);
	}

	/* Setup IRQ handler. */
	error = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_CLK | INTR_MPSAFE,
	    imsic_intr, NULL, sc, (void **)&sc);
	if (error != 0) {
		device_printf(dev, "Unable to setup IRQ resource\n");
		return (ENXIO);
	}

	OF_device_register_xref(xref, dev);

	return (0);
}

static int
imsic_find_msi(device_t dev, int cpu, int *msi)
{
	struct imsic_softc *sc;

	sc = device_get_softc(dev);

	IMSIC_LOCK(sc, cpu);
	if (IMSIC_ISEMPTY(&sc->pcpu_data[cpu].allocated_msis)) {
		/* No MSIs left to allocate. */
		IMSIC_UNLOCK(sc, cpu);
		return (ESRCH);
	}
	/* Find free MSI (first bit set). */
	*msi = IMSIC_FFS(&sc->pcpu_data[cpu].allocated_msis) - 1;
	/* Clear the bit to mark as allocated. */
	IMSIC_CLR(*msi, &sc->pcpu_data[cpu].allocated_msis);
	IMSIC_UNLOCK(sc, cpu);

	return (0);
}

int
imsic_alloc_aplic_msi(device_t dev, int cpu, int aplic_irq, int *msi)
{
	struct imsic_softc *sc;
	int error;
	int m;

	sc = device_get_softc(dev);

	error = imsic_find_msi(dev, cpu, &m);
	if (error != 0)
		return (error);

	if (msi != NULL)
		*msi = m;

	sc->pcpu_data[cpu].aplic_irq[m] = aplic_irq;
	imsic_enable_irq(dev, cpu, m);

	return (0);
}

static int
imsic_alloc_msi(device_t dev, int cpu, struct intr_irqsrc **isrc)
{
	struct imsic_softc *sc;
	struct imsic_irq *irq;
	const char *name;
	int error;
	int m;

	sc = device_get_softc(dev);

	error = imsic_find_msi(dev, cpu, &m);
	if (error != 0)
		return (error);

	irq = sc->pcpu_data[cpu].irqs[m];
	if (irq == NULL)
		irq = malloc(sizeof(struct imsic_irq), M_DEVBUF,
		    M_ZERO | M_WAITOK);
	irq->msi = m;
	irq->cpuid = cpu;
	sc->pcpu_data[cpu].irqs[m] = irq;

	name = device_get_nameunit(dev);
	error = intr_isrc_register(&irq->isrc, sc->dev, INTR_ISRCF_PPI,
	    "%s,%d", name, m);
	if (error != 0)
		return (error);

	if (isrc != NULL)
		*isrc = &irq->isrc;

	return (0);
}

static void
imsic_release_msi(device_t dev, struct imsic_irq *src)
{
	struct imsic_softc *sc;
	struct imsic_irq *irq;
	int error;
	int cpu;
	int m;

	sc = device_get_softc(dev);
	cpu = src->cpuid;
	m = src->msi;

	irq = sc->pcpu_data[cpu].irqs[m];

	error = intr_isrc_deregister(&irq->isrc);
	if (error) {
		device_printf(dev, "could not deregister irq\n");
		return;
	}

	free(irq, M_DEVBUF);
	sc->pcpu_data[cpu].irqs[m] = NULL;

	/* The MSI bit m is now available on this cpu. */
	IMSIC_LOCK(sc, cpu);
	IMSIC_SET(m, &sc->pcpu_data[cpu].allocated_msis);
	IMSIC_UNLOCK(sc, cpu);
}

static void
imsic_pic_disable_intr(device_t dev, struct intr_irqsrc *isrc)
{
	struct imsic_irq *src;

	src = ISRC_TO_IRQ(isrc);

	/* Disable the interrupt source */
	imsic_disable_interrupt(src->msi);
}

static void
imsic_pic_enable_intr(device_t dev, struct intr_irqsrc *isrc)
{
	struct imsic_irq *src;

	src = ISRC_TO_IRQ(isrc);

	/* Enable the interrupt source */
	imsic_enable_interrupt(src->msi);
}

static void
imsic_pic_pre_ithread(device_t dev, struct intr_irqsrc *isrc)
{
	struct imsic_irq *src;

	src = ISRC_TO_IRQ(isrc);

	imsic_disable_irq(dev, src->cpuid, src->msi);
}

static void
imsic_pic_post_ithread(device_t dev, struct intr_irqsrc *isrc)
{
	struct imsic_irq *src;

	src = ISRC_TO_IRQ(isrc);

	imsic_enable_irq(dev, src->cpuid, src->msi);
}

#ifdef SMP
static void
imsic_pic_init_secondary(device_t dev, uint32_t rootnum)
{

	/*
	 * Enable interrupt delivery by interrupt file. APLIC will forward the
	 * interrupts to IMSIC as MSI writes.
	 */
	imsic_csr_write(IMSIC_EIDELIVERY, EIDELIVERY_EN);

	/* Receive all interrupts */
	imsic_csr_write(IMSIC_EITHRESHOLD, ITHRESHOLD_DIS);
}
#endif

static int
imsic_msi_alloc_msi(device_t dev, device_t child, int count, int maxcount,
    device_t *pic, struct intr_irqsrc **isrc)
{
	struct imsic_irq *src;
	int error;
	int cpu;
	int i;
	int j;

	cpu = PCPU_GET(cpuid);

	for (i = 0; i < count; i++) {
		error = imsic_alloc_msi(dev, cpu, &isrc[i]);
		if (error) {
			for (j = 0; j < i; j++) {
				src = ISRC_TO_IRQ(isrc[j]);
				imsic_release_msi(dev, src);
			}
			return (error);
		}
	}

	return (0);
}

static int
imsic_msi_release_msi(device_t dev, device_t child, int count,
    struct intr_irqsrc **isrc)
{
	struct imsic_irq *src;
	int i;

	for (i = 0; i < count; i++) {
		src = ISRC_TO_IRQ(isrc[i]);
		imsic_release_msi(dev, src);
	}

	return (0);
}

static int
imsic_msi_alloc_msix(device_t dev, device_t child, device_t *pic,
    struct intr_irqsrc **isrc)
{
	int error;
	int cpu;

	cpu = PCPU_GET(cpuid);

	error = imsic_alloc_msi(dev, cpu, isrc);
	if (error)
		return (error);

	return (0);
}

static int
imsic_msi_release_msix(device_t dev, device_t child, struct intr_irqsrc *isrc)
{
	struct imsic_irq *src;

	src = ISRC_TO_IRQ(isrc);

	imsic_release_msi(dev, src);

	return (0);
}

static int
imsic_msi_map_msi(device_t dev, device_t child, struct intr_irqsrc *isrc,
    uint64_t *addr, uint32_t *data)
{
	struct imsic_softc *sc;
	struct imsic_irq *irq;
	vm_paddr_t msi_addr;
	uint32_t hartid;

	sc = device_get_softc(dev);
	irq = (struct imsic_irq *)isrc;
	hartid = riscv_cpu_to_hartid(irq->cpuid);

	msi_addr = (sc->msi_config.msi_base >> IMSIC_MSI_PPN_SHIFT);
	msi_addr |= ((hartid & ((1 << sc->msi_config.lhxw) - 1))
	    << sc->msi_config.lhxs);
	msi_addr |= (((hartid >> sc->msi_config.lhxw) &
	    ((1 << sc->msi_config.hhxw) - 1)) << sc->msi_config.hhxs);
	msi_addr <<= IMSIC_MSI_PPN_SHIFT;

	*addr = msi_addr;
	*data = irq->msi;

	return (0);
}

static device_method_t imsic_methods[] = {
	DEVMETHOD(device_probe,		imsic_probe),
	DEVMETHOD(device_attach,	imsic_attach),

	DEVMETHOD(pic_disable_intr,	imsic_pic_disable_intr),
	DEVMETHOD(pic_enable_intr,	imsic_pic_enable_intr),
	DEVMETHOD(pic_pre_ithread,	imsic_pic_pre_ithread),
	DEVMETHOD(pic_post_ithread,	imsic_pic_post_ithread),
	DEVMETHOD(pic_post_filter,	imsic_pic_post_ithread),
#ifdef SMP
	DEVMETHOD(pic_init_secondary,	imsic_pic_init_secondary),
#endif

	/* MSI/MSI-X */
	DEVMETHOD(msi_alloc_msi,	imsic_msi_alloc_msi),
	DEVMETHOD(msi_release_msi,	imsic_msi_release_msi),
	DEVMETHOD(msi_alloc_msix,	imsic_msi_alloc_msix),
	DEVMETHOD(msi_release_msix,	imsic_msi_release_msix),
	DEVMETHOD(msi_map_msi,		imsic_msi_map_msi),

	DEVMETHOD_END
};

DEFINE_CLASS_0(imsic, imsic_driver, imsic_methods, sizeof(struct imsic_softc));
EARLY_DRIVER_MODULE(imsic, intc, imsic_driver, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_EARLY);
