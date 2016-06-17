/* $Id: pci_impl.h,v 1.9 2001/06/13 06:34:30 davem Exp $
 * pci_impl.h: Helper definitions for PCI controller support.
 *
 * Copyright (C) 1999 David S. Miller (davem@redhat.com)
 */

#ifndef PCI_IMPL_H
#define PCI_IMPL_H

#include <linux/types.h>
#include <linux/spinlock.h>
#include <asm/io.h>

extern spinlock_t pci_controller_lock;
extern struct pci_controller_info *pci_controller_root;

extern struct pci_pbm_info *pci_bus2pbm[256];
extern unsigned char pci_highest_busnum;
extern int pci_num_controllers;

/* PCI bus scanning and fixup support. */
extern void pci_fixup_host_bridge_self(struct pci_bus *pbus);
extern void pci_fill_in_pbm_cookies(struct pci_bus *pbus,
				    struct pci_pbm_info *pbm,
				    int prom_node);
extern void pci_record_assignments(struct pci_pbm_info *pbm,
				   struct pci_bus *pbus);
extern void pci_assign_unassigned(struct pci_pbm_info *pbm,
				  struct pci_bus *pbus);
extern void pci_fixup_irq(struct pci_pbm_info *pbm,
			  struct pci_bus *pbus);
extern void pci_determine_66mhz_disposition(struct pci_pbm_info *pbm,
					    struct pci_bus *pbus);
extern void pci_setup_busmastering(struct pci_pbm_info *pbm,
				   struct pci_bus *pbus);
extern void pci_register_legacy_regions(struct resource *io_res,
					struct resource *mem_res);

/* Error reporting support. */
extern void pci_scan_for_target_abort(struct pci_controller_info *, struct pci_pbm_info *, struct pci_bus *);
extern void pci_scan_for_master_abort(struct pci_controller_info *, struct pci_pbm_info *, struct pci_bus *);
extern void pci_scan_for_parity_error(struct pci_controller_info *, struct pci_pbm_info *, struct pci_bus *);

/* Configuration space access. */
extern spinlock_t pci_poke_lock;
extern volatile int pci_poke_in_progress;
extern volatile int pci_poke_cpu;
extern volatile int pci_poke_faulted;

static __inline__ void pci_config_read8(u8 *addr, u8 *ret)
{
	unsigned long flags;
	u8 byte;

	spin_lock_irqsave(&pci_poke_lock, flags);
	pci_poke_cpu = smp_processor_id();
	pci_poke_in_progress = 1;
	pci_poke_faulted = 0;
	__asm__ __volatile__("membar #Sync\n\t"
			     "lduba [%1] %2, %0\n\t"
			     "membar #Sync"
			     : "=r" (byte)
			     : "r" (addr), "i" (ASI_PHYS_BYPASS_EC_E_L)
			     : "memory");
	pci_poke_in_progress = 0;
	pci_poke_cpu = -1;
	if (!pci_poke_faulted)
		*ret = byte;
	spin_unlock_irqrestore(&pci_poke_lock, flags);
}

static __inline__ void pci_config_read16(u16 *addr, u16 *ret)
{
	unsigned long flags;
	u16 word;

	spin_lock_irqsave(&pci_poke_lock, flags);
	pci_poke_cpu = smp_processor_id();
	pci_poke_in_progress = 1;
	pci_poke_faulted = 0;
	__asm__ __volatile__("membar #Sync\n\t"
			     "lduha [%1] %2, %0\n\t"
			     "membar #Sync"
			     : "=r" (word)
			     : "r" (addr), "i" (ASI_PHYS_BYPASS_EC_E_L)
			     : "memory");
	pci_poke_in_progress = 0;
	pci_poke_cpu = -1;
	if (!pci_poke_faulted)
		*ret = word;
	spin_unlock_irqrestore(&pci_poke_lock, flags);
}

static __inline__ void pci_config_read32(u32 *addr, u32 *ret)
{
	unsigned long flags;
	u32 dword;

	spin_lock_irqsave(&pci_poke_lock, flags);
	pci_poke_cpu = smp_processor_id();
	pci_poke_in_progress = 1;
	pci_poke_faulted = 0;
	__asm__ __volatile__("membar #Sync\n\t"
			     "lduwa [%1] %2, %0\n\t"
			     "membar #Sync"
			     : "=r" (dword)
			     : "r" (addr), "i" (ASI_PHYS_BYPASS_EC_E_L)
			     : "memory");
	pci_poke_in_progress = 0;
	pci_poke_cpu = -1;
	if (!pci_poke_faulted)
		*ret = dword;
	spin_unlock_irqrestore(&pci_poke_lock, flags);
}

static __inline__ void pci_config_write8(u8 *addr, u8 val)
{
	unsigned long flags;

	spin_lock_irqsave(&pci_poke_lock, flags);
	pci_poke_cpu = smp_processor_id();
	pci_poke_in_progress = 1;
	pci_poke_faulted = 0;
	__asm__ __volatile__("membar #Sync\n\t"
			     "stba %0, [%1] %2\n\t"
			     "membar #Sync"
			     : /* no outputs */
			     : "r" (val), "r" (addr), "i" (ASI_PHYS_BYPASS_EC_E_L)
			     : "memory");
	pci_poke_in_progress = 0;
	pci_poke_cpu = -1;
	spin_unlock_irqrestore(&pci_poke_lock, flags);
}

static __inline__ void pci_config_write16(u16 *addr, u16 val)
{
	unsigned long flags;

	spin_lock_irqsave(&pci_poke_lock, flags);
	pci_poke_cpu = smp_processor_id();
	pci_poke_in_progress = 1;
	pci_poke_faulted = 0;
	__asm__ __volatile__("membar #Sync\n\t"
			     "stha %0, [%1] %2\n\t"
			     "membar #Sync"
			     : /* no outputs */
			     : "r" (val), "r" (addr), "i" (ASI_PHYS_BYPASS_EC_E_L)
			     : "memory");
	pci_poke_in_progress = 0;
	pci_poke_cpu = -1;
	spin_unlock_irqrestore(&pci_poke_lock, flags);
}

static __inline__ void pci_config_write32(u32 *addr, u32 val)
{
	unsigned long flags;

	spin_lock_irqsave(&pci_poke_lock, flags);
	pci_poke_cpu = smp_processor_id();
	pci_poke_in_progress = 1;
	pci_poke_faulted = 0;
	__asm__ __volatile__("membar #Sync\n\t"
			     "stwa %0, [%1] %2\n\t"
			     "membar #Sync"
			     : /* no outputs */
			     : "r" (val), "r" (addr), "i" (ASI_PHYS_BYPASS_EC_E_L)
			     : "memory");
	pci_poke_in_progress = 0;
	pci_poke_cpu = -1;
	spin_unlock_irqrestore(&pci_poke_lock, flags);
}

#endif /* !(PCI_IMPL_H) */
