/*
 * linux/arch/alpha/kernel/ksyms.c
 *
 * Export the alpha-specific functions that are needed for loadable
 * modules.
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/user.h>
#include <linux/elfcore.h>
#include <linux/socket.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <linux/pci.h>
#include <linux/tty.h>
#include <linux/mm.h>
#include <linux/delay.h>

#include <asm/io.h>
#include <asm/console.h>
#include <asm/hwrpb.h>
#include <asm/uaccess.h>
#include <asm/processor.h>
#include <asm/checksum.h>
#include <linux/interrupt.h>
#include <asm/softirq.h>
#include <asm/fpu.h>
#include <asm/irq.h>
#include <asm/machvec.h>
#include <asm/pgalloc.h>
#include <asm/semaphore.h>

#define __KERNEL_SYSCALLS__
#include <asm/unistd.h>

extern struct hwrpb_struct *hwrpb;
extern void dump_thread(struct pt_regs *, struct user *);
extern void dump_elf_thread(elf_gregset_t dest, struct pt_regs *pt,
			    struct task_struct *task);
extern int dump_fpu(struct pt_regs *, elf_fpregset_t *);
extern spinlock_t kernel_flag;
extern spinlock_t rtc_lock;

/* these are C runtime functions with special calling conventions: */
extern void __divl (void);
extern void __reml (void);
extern void __divq (void);
extern void __remq (void);
extern void __divlu (void);
extern void __remlu (void);
extern void __divqu (void);
extern void __remqu (void);

EXPORT_SYMBOL(alpha_mv);
EXPORT_SYMBOL(enable_irq);
EXPORT_SYMBOL(disable_irq);
EXPORT_SYMBOL(disable_irq_nosync);
EXPORT_SYMBOL(probe_irq_mask);
EXPORT_SYMBOL(screen_info);
EXPORT_SYMBOL(perf_irq);
EXPORT_SYMBOL(callback_getenv);
EXPORT_SYMBOL(callback_setenv);
EXPORT_SYMBOL(callback_save_env);
#ifdef CONFIG_ALPHA_GENERIC
EXPORT_SYMBOL(alpha_using_srm);
#endif /* CNFIG_ALPHA_GENERIC */

/* platform dependent support */
EXPORT_SYMBOL(_inb);
EXPORT_SYMBOL(_inw);
EXPORT_SYMBOL(_inl);
EXPORT_SYMBOL(_outb);
EXPORT_SYMBOL(_outw);
EXPORT_SYMBOL(_outl);
EXPORT_SYMBOL(_readb);
EXPORT_SYMBOL(_readw);
EXPORT_SYMBOL(_readl);
EXPORT_SYMBOL(_writeb);
EXPORT_SYMBOL(_writew);
EXPORT_SYMBOL(_writel);
EXPORT_SYMBOL(___raw_readb); 
EXPORT_SYMBOL(___raw_readw); 
EXPORT_SYMBOL(___raw_readl); 
EXPORT_SYMBOL(___raw_readq); 
EXPORT_SYMBOL(___raw_writeb); 
EXPORT_SYMBOL(___raw_writew); 
EXPORT_SYMBOL(___raw_writel); 
EXPORT_SYMBOL(___raw_writeq); 
EXPORT_SYMBOL(_memcpy_fromio);
EXPORT_SYMBOL(_memcpy_toio);
EXPORT_SYMBOL(_memset_c_io);
EXPORT_SYMBOL(insb);
EXPORT_SYMBOL(insw);
EXPORT_SYMBOL(insl);
EXPORT_SYMBOL(outsb);
EXPORT_SYMBOL(outsw);
EXPORT_SYMBOL(outsl);
EXPORT_SYMBOL(strcat);
EXPORT_SYMBOL(strcmp);
EXPORT_SYMBOL(strcpy);
EXPORT_SYMBOL(strlen);
EXPORT_SYMBOL(strncmp);
EXPORT_SYMBOL(strncpy);
EXPORT_SYMBOL(strnlen);
EXPORT_SYMBOL(strncat);
EXPORT_SYMBOL(strstr);
EXPORT_SYMBOL(strtok);
EXPORT_SYMBOL(strpbrk);
EXPORT_SYMBOL(strchr);
EXPORT_SYMBOL(strrchr);
EXPORT_SYMBOL(memcmp);
EXPORT_SYMBOL(memmove);
EXPORT_SYMBOL(memscan);
EXPORT_SYMBOL(__memcpy);
EXPORT_SYMBOL(__memset);
EXPORT_SYMBOL(__memsetw);
EXPORT_SYMBOL(__constant_c_memset);
EXPORT_SYMBOL(copy_page);
EXPORT_SYMBOL(clear_page);

EXPORT_SYMBOL(__delay);
EXPORT_SYMBOL(ndelay);
EXPORT_SYMBOL(udelay);

EXPORT_SYMBOL(__direct_map_base);
EXPORT_SYMBOL(__direct_map_size);

#ifdef CONFIG_PCI
EXPORT_SYMBOL(pci_alloc_consistent);
EXPORT_SYMBOL(pci_free_consistent);
EXPORT_SYMBOL(pci_map_single);
EXPORT_SYMBOL(pci_map_page);
EXPORT_SYMBOL(pci_unmap_single);
EXPORT_SYMBOL(pci_unmap_page);
EXPORT_SYMBOL(pci_map_sg);
EXPORT_SYMBOL(pci_unmap_sg);
EXPORT_SYMBOL(pci_dma_supported);
EXPORT_SYMBOL(pci_dac_dma_supported);
EXPORT_SYMBOL(pci_dac_page_to_dma);
EXPORT_SYMBOL(pci_dac_dma_to_page);
EXPORT_SYMBOL(pci_dac_dma_to_offset);
#endif

EXPORT_SYMBOL(dump_thread);
EXPORT_SYMBOL(dump_elf_thread);
EXPORT_SYMBOL(dump_fpu);
EXPORT_SYMBOL(hwrpb);
EXPORT_SYMBOL(wrusp);
EXPORT_SYMBOL(start_thread);
EXPORT_SYMBOL(alpha_read_fp_reg);
EXPORT_SYMBOL(alpha_read_fp_reg_s);
EXPORT_SYMBOL(alpha_write_fp_reg);
EXPORT_SYMBOL(alpha_write_fp_reg_s);

/* In-kernel system calls.  */
EXPORT_SYMBOL(kernel_thread);
EXPORT_SYMBOL(sys_open);
EXPORT_SYMBOL(sys_dup);
EXPORT_SYMBOL(sys_exit);
EXPORT_SYMBOL(sys_write);
EXPORT_SYMBOL(sys_read);
EXPORT_SYMBOL(sys_lseek);
EXPORT_SYMBOL(__kernel_execve);
EXPORT_SYMBOL(sys_setsid);
EXPORT_SYMBOL(sys_sync);
EXPORT_SYMBOL(sys_wait4);

/* Networking helper routines. */
EXPORT_SYMBOL(csum_tcpudp_magic);
EXPORT_SYMBOL(ip_compute_csum);
EXPORT_SYMBOL(ip_fast_csum);
EXPORT_SYMBOL(csum_partial_copy);
EXPORT_SYMBOL(csum_partial_copy_nocheck);
EXPORT_SYMBOL(csum_partial_copy_from_user);
EXPORT_SYMBOL(csum_ipv6_magic);

#ifdef CONFIG_MATHEMU_MODULE
extern long (*alpha_fp_emul_imprecise)(struct pt_regs *, unsigned long);
extern long (*alpha_fp_emul) (unsigned long pc);
EXPORT_SYMBOL(alpha_fp_emul_imprecise);
EXPORT_SYMBOL(alpha_fp_emul);
#endif

#ifdef CONFIG_ALPHA_BROKEN_IRQ_MASK
EXPORT_SYMBOL(__min_ipl);
#endif

/*
 * The following are specially called from the uaccess assembly stubs.
 */
EXPORT_SYMBOL_NOVERS(__copy_user);
EXPORT_SYMBOL_NOVERS(__do_clear_user);
EXPORT_SYMBOL(__strncpy_from_user);
EXPORT_SYMBOL(__strnlen_user);

/* Semaphore helper functions.  */
EXPORT_SYMBOL(__down_failed);
EXPORT_SYMBOL(__down_failed_interruptible);
EXPORT_SYMBOL(__up_wakeup);
EXPORT_SYMBOL(down);
EXPORT_SYMBOL(down_interruptible);
EXPORT_SYMBOL(down_trylock);
EXPORT_SYMBOL(up);

/* 
 * SMP-specific symbols.
 */

#ifdef CONFIG_SMP
EXPORT_SYMBOL(kernel_flag);
EXPORT_SYMBOL(synchronize_irq);
EXPORT_SYMBOL(flush_tlb_all);
EXPORT_SYMBOL(flush_tlb_mm);
EXPORT_SYMBOL(flush_tlb_range);
EXPORT_SYMBOL(flush_tlb_page);
EXPORT_SYMBOL(smp_imb);
EXPORT_SYMBOL(cpu_data);
EXPORT_SYMBOL(__cpu_number_map);
EXPORT_SYMBOL(__cpu_logical_map);
EXPORT_SYMBOL(smp_num_cpus);
EXPORT_SYMBOL(smp_call_function);
EXPORT_SYMBOL(smp_call_function_on_cpu);
EXPORT_SYMBOL(global_irq_holder);
EXPORT_SYMBOL(__global_cli);
EXPORT_SYMBOL(__global_sti);
EXPORT_SYMBOL(__global_save_flags);
EXPORT_SYMBOL(__global_restore_flags);
EXPORT_SYMBOL(atomic_dec_and_lock);
#ifdef CONFIG_DEBUG_SPINLOCK
EXPORT_SYMBOL(spin_unlock);
EXPORT_SYMBOL(debug_spin_lock);
EXPORT_SYMBOL(debug_spin_trylock);
#endif
#ifdef CONFIG_DEBUG_RWLOCK
EXPORT_SYMBOL(write_lock);
EXPORT_SYMBOL(read_lock);
#endif
EXPORT_SYMBOL(cpu_present_mask);
#endif /* CONFIG_SMP */

/*
 * NUMA specific symbols
 */
#ifdef CONFIG_DISCONTIGMEM
EXPORT_SYMBOL(plat_node_data);
#endif /* CONFIG_DISCONTIGMEM */

EXPORT_SYMBOL(rtc_lock);

/*
 * The following are special because they're not called
 * explicitly (the C compiler or assembler generates them in
 * response to division operations).  Fortunately, their
 * interface isn't gonna change any time soon now, so it's OK
 * to leave it out of version control.
 */
# undef memcpy
# undef memset
EXPORT_SYMBOL_NOVERS(__divl);
EXPORT_SYMBOL_NOVERS(__divlu);
EXPORT_SYMBOL_NOVERS(__divq);
EXPORT_SYMBOL_NOVERS(__divqu);
EXPORT_SYMBOL_NOVERS(__reml);
EXPORT_SYMBOL_NOVERS(__remlu);
EXPORT_SYMBOL_NOVERS(__remq);
EXPORT_SYMBOL_NOVERS(__remqu);
EXPORT_SYMBOL_NOVERS(memcpy);
EXPORT_SYMBOL_NOVERS(memset);
EXPORT_SYMBOL_NOVERS(memchr);

EXPORT_SYMBOL(get_wchan);

#ifdef CONFIG_ALPHA_IRONGATE
EXPORT_SYMBOL(irongate_ioremap);
EXPORT_SYMBOL(irongate_iounmap);
#endif
#ifdef CONFIG_ALPHA_TITAN
EXPORT_SYMBOL(titan_ioremap);
EXPORT_SYMBOL(titan_iounmap);
#endif
#ifdef CONFIG_ALPHA_MARVEL
EXPORT_SYMBOL(marvel_ioremap);
EXPORT_SYMBOL(marvel_iounmap);
#endif
