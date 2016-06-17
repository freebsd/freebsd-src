/*
 * Architecture-specific kernel symbols
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/string.h>
EXPORT_SYMBOL_NOVERS(memscan);
EXPORT_SYMBOL_NOVERS(memset);
EXPORT_SYMBOL(memcmp);
EXPORT_SYMBOL_NOVERS(memcpy);
EXPORT_SYMBOL(memmove);
EXPORT_SYMBOL(strcat);
EXPORT_SYMBOL(strchr);
EXPORT_SYMBOL(strrchr);
EXPORT_SYMBOL(strcmp);
EXPORT_SYMBOL(strcpy);
EXPORT_SYMBOL(strlen);
EXPORT_SYMBOL(strnlen);
EXPORT_SYMBOL(strncat);
EXPORT_SYMBOL(strncmp);
EXPORT_SYMBOL(strncpy);
EXPORT_SYMBOL(strtok);
EXPORT_SYMBOL(strstr);

#include <asm/hardware.h>	/* struct parisc_device for asm/pci.h */
#include <linux/pci.h>
EXPORT_SYMBOL(hppa_dma_ops);
#if defined(CONFIG_PCI) || defined(CONFIG_ISA)
EXPORT_SYMBOL(get_pci_node_path);
#endif

#ifdef CONFIG_IOMMU_CCIO
EXPORT_SYMBOL(ccio_get_fake);
#endif

#include <linux/sched.h>
#include <asm/irq.h>
EXPORT_SYMBOL(enable_irq);
EXPORT_SYMBOL(disable_irq);

#include <asm/processor.h>
EXPORT_SYMBOL(kernel_thread);
EXPORT_SYMBOL(boot_cpu_data);
EXPORT_SYMBOL(map_hpux_gateway_page);
#ifdef CONFIG_EISA
EXPORT_SYMBOL(EISA_bus);
#endif

#include <linux/pm.h>
EXPORT_SYMBOL(pm_power_off);

#ifdef CONFIG_SMP
EXPORT_SYMBOL(synchronize_irq);

#include <asm/smplock.h>
EXPORT_SYMBOL(kernel_flag);

/* from asm/system.h */
#include <asm/system.h>
EXPORT_SYMBOL(__global_sti);
EXPORT_SYMBOL(__global_cli);
EXPORT_SYMBOL(__global_save_flags);
EXPORT_SYMBOL(__global_restore_flags);

#include <linux/smp.h>
EXPORT_SYMBOL(smp_num_cpus);
EXPORT_SYMBOL(smp_call_function);
#endif /* CONFIG_SMP */

#include <asm/atomic.h>
EXPORT_SYMBOL(__xchg8);
EXPORT_SYMBOL(__xchg32);
EXPORT_SYMBOL(__cmpxchg_u32);
#ifdef CONFIG_SMP
EXPORT_SYMBOL(__atomic_hash);
#endif
#ifdef __LP64__
EXPORT_SYMBOL(__xchg64);
EXPORT_SYMBOL(__cmpxchg_u64);
#endif

#include <asm/uaccess.h>
EXPORT_SYMBOL(lcopy_to_user);
EXPORT_SYMBOL(lcopy_from_user);
EXPORT_SYMBOL(lstrnlen_user);
EXPORT_SYMBOL(lclear_user);

#ifndef __LP64__
/* Needed so insmod can set dp value */
extern int $global$;
EXPORT_SYMBOL_NOVERS($global$);
#endif

#include <asm/gsc.h>
EXPORT_SYMBOL(register_parisc_driver);
EXPORT_SYMBOL(unregister_parisc_driver);
EXPORT_SYMBOL(pdc_iodc_read);
#ifdef CONFIG_GSC
EXPORT_SYMBOL(gsc_alloc_irq);
#endif

#include <asm/io.h>
EXPORT_SYMBOL(__ioremap);
EXPORT_SYMBOL(iounmap);
EXPORT_SYMBOL(memcpy_toio);
EXPORT_SYMBOL(memcpy_fromio);
EXPORT_SYMBOL(memset_io);

#if defined(CONFIG_PCI) || defined(CONFIG_ISA)
EXPORT_SYMBOL(inb);
EXPORT_SYMBOL(inw);
EXPORT_SYMBOL(inl);
EXPORT_SYMBOL(outb);
EXPORT_SYMBOL(outw);
EXPORT_SYMBOL(outl);

EXPORT_SYMBOL(insb);
EXPORT_SYMBOL(insw);
EXPORT_SYMBOL(insl);
EXPORT_SYMBOL(outsb);
EXPORT_SYMBOL(outsw);
EXPORT_SYMBOL(outsl);
#endif

#include <asm/cache.h>
EXPORT_SYMBOL(flush_kernel_dcache_range_asm);
EXPORT_SYMBOL(flush_kernel_dcache_page);

/* asm/pgalloc.h doesn't include all it's dependencies */
extern void __flush_dcache_page(struct page *page);
EXPORT_SYMBOL(__flush_dcache_page);

extern void flush_cache_all_local(void);
EXPORT_SYMBOL(flush_cache_all_local);

#include <asm/unistd.h>
extern long sys_open(const char *, int, int);
extern off_t sys_lseek(int, off_t, int);
extern int sys_read(int, char *, int);
extern int sys_write(int, const char *, int);
EXPORT_SYMBOL(sys_open);
EXPORT_SYMBOL(sys_lseek);
EXPORT_SYMBOL(sys_read);
EXPORT_SYMBOL(sys_write);

#include <asm/semaphore.h>
EXPORT_SYMBOL(__up);
EXPORT_SYMBOL(__down_interruptible);
EXPORT_SYMBOL(__down);

#include <linux/in6.h>
#include <asm/checksum.h>
EXPORT_SYMBOL(csum_partial_copy);

#include <asm/pdc.h>
EXPORT_SYMBOL(pdc_add_valid);
EXPORT_SYMBOL(pdc_tod_read);
EXPORT_SYMBOL(pdc_tod_set);
EXPORT_SYMBOL(pdc_lan_station_id);
EXPORT_SYMBOL(pdc_get_initiator);

extern void $$divI(void);
extern void $$divU(void);
extern void $$remI(void);
extern void $$remU(void);
extern void $$mulI(void);
extern void $$mulU(void);
extern void $$divU_3(void);
extern void $$divU_5(void);
extern void $$divU_6(void);
extern void $$divU_9(void);
extern void $$divU_10(void);
extern void $$divU_12(void);
extern void $$divU_7(void);
extern void $$divU_14(void);
extern void $$divU_15(void);
extern void $$divI_3(void);
extern void $$divI_5(void);
extern void $$divI_6(void);
extern void $$divI_7(void);
extern void $$divI_9(void);
extern void $$divI_10(void);
extern void $$divI_12(void);
extern void $$divI_14(void);
extern void $$divI_15(void);

EXPORT_SYMBOL_NOVERS($$divI);
EXPORT_SYMBOL_NOVERS($$divU);
EXPORT_SYMBOL_NOVERS($$remI);
EXPORT_SYMBOL_NOVERS($$remU);
EXPORT_SYMBOL_NOVERS($$mulI);
#ifndef __LP64__
EXPORT_SYMBOL_NOVERS($$mulU);
#endif
EXPORT_SYMBOL_NOVERS($$divU_3);
EXPORT_SYMBOL_NOVERS($$divU_5);
EXPORT_SYMBOL_NOVERS($$divU_6);
EXPORT_SYMBOL_NOVERS($$divU_9);
EXPORT_SYMBOL_NOVERS($$divU_10);
EXPORT_SYMBOL_NOVERS($$divU_12);
EXPORT_SYMBOL_NOVERS($$divU_7);
EXPORT_SYMBOL_NOVERS($$divU_14);
EXPORT_SYMBOL_NOVERS($$divU_15);
EXPORT_SYMBOL_NOVERS($$divI_3);
EXPORT_SYMBOL_NOVERS($$divI_5);
EXPORT_SYMBOL_NOVERS($$divI_6);
EXPORT_SYMBOL_NOVERS($$divI_7);
EXPORT_SYMBOL_NOVERS($$divI_9);
EXPORT_SYMBOL_NOVERS($$divI_10);
EXPORT_SYMBOL_NOVERS($$divI_12);
EXPORT_SYMBOL_NOVERS($$divI_14);
EXPORT_SYMBOL_NOVERS($$divI_15);

extern void __ashrdi3(void);
extern void __ashldi3(void);
extern void __lshrdi3(void);
extern void __muldi3(void);

EXPORT_SYMBOL_NOVERS(__ashrdi3);
EXPORT_SYMBOL_NOVERS(__ashldi3);
EXPORT_SYMBOL_NOVERS(__lshrdi3);
EXPORT_SYMBOL_NOVERS(__muldi3);

#ifdef __LP64__
extern void __divdi3(void);
extern void __udivdi3(void);
extern void __umoddi3(void);
extern void __moddi3(void);

EXPORT_SYMBOL_NOVERS(__divdi3);
EXPORT_SYMBOL_NOVERS(__udivdi3);
EXPORT_SYMBOL_NOVERS(__umoddi3);
EXPORT_SYMBOL_NOVERS(__moddi3);
#endif

#ifndef __LP64__
extern void $$dyncall(void);
EXPORT_SYMBOL_NOVERS($$dyncall);
#endif

#ifdef CONFIG_SMP
#ifdef CONFIG_DEBUG_SPINLOCK
#include <asm/spinlock.h>
EXPORT_SYMBOL(spin_lock);
EXPORT_SYMBOL(spin_unlock);
EXPORT_SYMBOL(spin_trylock);
#endif
#endif
