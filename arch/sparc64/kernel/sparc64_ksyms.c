/* $Id: sparc64_ksyms.c,v 1.119.2.2 2002/03/14 01:26:21 kanoj Exp $
 * arch/sparc64/kernel/sparc64_ksyms.c: Sparc64 specific ksyms support.
 *
 * Copyright (C) 1996 David S. Miller (davem@caip.rutgers.edu)
 * Copyright (C) 1996 Eddie C. Dost (ecd@skynet.be)
 * Copyright (C) 1999 Jakub Jelinek (jj@ultra.linux.cz)
 */

/* Tell string.h we don't want memcpy etc. as cpp defines */
#define EXPORT_SYMTAB_STROPS
#define PROMLIB_INTERNAL

#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/in6.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/fs_struct.h>
#include <linux/mm.h>

#include <asm/oplib.h>
#include <asm/delay.h>
#include <asm/system.h>
#include <asm/auxio.h>
#include <asm/pgtable.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/softirq.h>
#include <asm/hardirq.h>
#include <asm/idprom.h>
#include <asm/svr4.h>
#include <asm/elf.h>
#include <asm/head.h>
#include <asm/smp.h>
#include <asm/mostek.h>
#include <asm/ptrace.h>
#include <asm/user.h>
#include <asm/uaccess.h>
#include <asm/checksum.h>
#include <asm/fpumacro.h>
#include <asm/pgalloc.h>
#ifdef CONFIG_SBUS
#include <asm/sbus.h>
#include <asm/dma.h>
#endif
#ifdef CONFIG_PCI
#include <asm/ebus.h>
#include <asm/isa.h>
#endif
#include <asm/a.out.h>
#include <asm/timer.h>

struct poll {
	int fd;
	short events;
	short revents;
};

extern unsigned prom_cpu_nodes[64];
extern void die_if_kernel(char *str, struct pt_regs *regs);
void _sigpause_common (unsigned int set, struct pt_regs *);
extern void *__bzero(void *, size_t);
extern void *__bzero_noasi(void *, size_t);
extern void *__memscan_zero(void *, size_t);
extern void *__memscan_generic(void *, int, size_t);
extern int __memcmp(const void *, const void *, __kernel_size_t);
extern int __strncmp(const char *, const char *, __kernel_size_t);
extern __kernel_size_t __strlen(const char *);
extern __kernel_size_t strlen(const char *);
extern char saved_command_line[];
extern void linux_sparc_syscall(void);
extern void rtrap(void);
extern void show_regs(struct pt_regs *);
extern void solaris_syscall(void);
extern void syscall_trace(void);
extern u32 sunos_sys_table[], sys_call_table32[];
extern void tl0_solaris(void);
extern void sys_sigsuspend(void);
extern int sys_getppid(void);
extern int svr4_getcontext(svr4_ucontext_t *uc, struct pt_regs *regs);
extern int svr4_setcontext(svr4_ucontext_t *uc, struct pt_regs *regs);
extern int sys_ioctl(unsigned int fd, unsigned int cmd, unsigned long arg);
extern int sys32_ioctl(unsigned int fd, unsigned int cmd, u32 arg);
extern int (*handle_mathemu)(struct pt_regs *, struct fpustate *);
extern long sparc32_open(const char * filename, int flags, int mode);
extern int register_ioctl32_conversion(unsigned int cmd, int (*handler)(unsigned int, unsigned int, unsigned long, struct file *));
extern int unregister_ioctl32_conversion(unsigned int cmd);
extern int io_remap_page_range(unsigned long from, unsigned long offset, unsigned long size, pgprot_t prot, int space);
                
extern int __ashrdi3(int, int);

extern void dump_thread(struct pt_regs *, struct user *);
extern int dump_fpu (struct pt_regs * regs, elf_fpregset_t * fpregs);

#ifdef CONFIG_SMP
extern spinlock_t kernel_flag;
extern int smp_num_cpus;
#ifdef CONFIG_DEBUG_SPINLOCK
extern void _do_spin_lock (spinlock_t *lock, char *str);
extern void _do_spin_unlock (spinlock_t *lock);
extern int _spin_trylock (spinlock_t *lock);
extern void _do_read_lock(rwlock_t *rw, char *str);
extern void _do_read_unlock(rwlock_t *rw, char *str);
extern void _do_write_lock(rwlock_t *rw, char *str);
extern void _do_write_unlock(rwlock_t *rw);
#endif
#endif

extern unsigned long phys_base;

/* used by various drivers */
#ifdef CONFIG_SMP
#ifndef CONFIG_DEBUG_SPINLOCK
/* Out of line rw-locking implementation. */
EXPORT_SYMBOL(__read_lock);
EXPORT_SYMBOL(__read_unlock);
EXPORT_SYMBOL(__write_lock);
EXPORT_SYMBOL(__write_unlock);
#endif

/* Kernel wide locking */
EXPORT_SYMBOL(kernel_flag);

/* Hard IRQ locking */
EXPORT_SYMBOL(global_irq_holder);
#ifdef CONFIG_SMP
EXPORT_SYMBOL(synchronize_irq);
#endif
EXPORT_SYMBOL(__global_cli);
EXPORT_SYMBOL(__global_sti);
EXPORT_SYMBOL(__global_save_flags);
EXPORT_SYMBOL(__global_restore_flags);

#if defined(CONFIG_MCOUNT)
extern void mcount(void);
EXPORT_SYMBOL_NOVERS(mcount);
#endif

/* Per-CPU information table */
EXPORT_SYMBOL(cpu_data);

/* Misc SMP information */
#ifdef CONFIG_SMP
EXPORT_SYMBOL(smp_num_cpus);
#endif
EXPORT_SYMBOL(__cpu_number_map);
EXPORT_SYMBOL(__cpu_logical_map);

/* Spinlock debugging library, optional. */
#ifdef CONFIG_DEBUG_SPINLOCK
EXPORT_SYMBOL(_do_spin_lock);
EXPORT_SYMBOL(_do_spin_unlock);
EXPORT_SYMBOL(_spin_trylock);
EXPORT_SYMBOL(_do_read_lock);
EXPORT_SYMBOL(_do_read_unlock);
EXPORT_SYMBOL(_do_write_lock);
EXPORT_SYMBOL(_do_write_unlock);
#endif

#ifdef CONFIG_SMP
EXPORT_SYMBOL(smp_call_function);
#endif

#endif

/* semaphores */
EXPORT_SYMBOL(__down);
EXPORT_SYMBOL(__down_interruptible);
EXPORT_SYMBOL(__up);

/* Atomic counter implementation. */
EXPORT_SYMBOL(__atomic_add);
EXPORT_SYMBOL(__atomic_sub);
#ifdef CONFIG_SMP
EXPORT_SYMBOL(atomic_dec_and_lock);
#endif

/* Atomic bit operations. */
EXPORT_SYMBOL(___test_and_set_bit);
EXPORT_SYMBOL(___test_and_clear_bit);
EXPORT_SYMBOL(___test_and_change_bit);
EXPORT_SYMBOL(___test_and_set_le_bit);
EXPORT_SYMBOL(___test_and_clear_le_bit);

EXPORT_SYMBOL(ivector_table);
EXPORT_SYMBOL(enable_irq);
EXPORT_SYMBOL(disable_irq);

EXPORT_SYMBOL(__flushw_user);

EXPORT_SYMBOL(tlb_type);
EXPORT_SYMBOL(get_fb_unmapped_area);
EXPORT_SYMBOL(flush_icache_range);
EXPORT_SYMBOL(flush_dcache_page);
EXPORT_SYMBOL(__flush_dcache_range);

EXPORT_SYMBOL(mostek_lock);
EXPORT_SYMBOL(mstk48t02_regs);
EXPORT_SYMBOL(request_fast_irq);
#if CONFIG_SUN_AUXIO
EXPORT_SYMBOL(auxio_set_led);
EXPORT_SYMBOL(auxio_set_lte);
#endif
#if CONFIG_SBUS
EXPORT_SYMBOL(sbus_root);
EXPORT_SYMBOL(dma_chain);
EXPORT_SYMBOL(sbus_set_sbus64);
EXPORT_SYMBOL(sbus_alloc_consistent);
EXPORT_SYMBOL(sbus_free_consistent);
EXPORT_SYMBOL(sbus_map_single);
EXPORT_SYMBOL(sbus_unmap_single);
EXPORT_SYMBOL(sbus_map_sg);
EXPORT_SYMBOL(sbus_unmap_sg);
EXPORT_SYMBOL(sbus_dma_sync_single);
EXPORT_SYMBOL(sbus_dma_sync_sg);
#endif
#ifdef CONFIG_PCI
EXPORT_SYMBOL(ebus_chain);
EXPORT_SYMBOL(isa_chain);
EXPORT_SYMBOL(pci_memspace_mask);
EXPORT_SYMBOL(outsb);
EXPORT_SYMBOL(outsw);
EXPORT_SYMBOL(outsl);
EXPORT_SYMBOL(insb);
EXPORT_SYMBOL(insw);
EXPORT_SYMBOL(insl);
EXPORT_SYMBOL(pci_alloc_consistent);
EXPORT_SYMBOL(pci_free_consistent);
EXPORT_SYMBOL(pci_map_single);
EXPORT_SYMBOL(pci_unmap_single);
EXPORT_SYMBOL(pci_map_sg);
EXPORT_SYMBOL(pci_unmap_sg);
EXPORT_SYMBOL(pci_dma_sync_single);
EXPORT_SYMBOL(pci_dma_sync_sg);
EXPORT_SYMBOL(pci_dma_supported);
#endif

/* IOCTL32 emulation hooks. */
EXPORT_SYMBOL(register_ioctl32_conversion);
EXPORT_SYMBOL(unregister_ioctl32_conversion);

/* I/O device mmaping on Sparc64. */
EXPORT_SYMBOL(io_remap_page_range);

/* Solaris/SunOS binary compatibility */
EXPORT_SYMBOL(_sigpause_common);

/* Should really be in linux/kernel/ksyms.c */
EXPORT_SYMBOL(dump_thread);
EXPORT_SYMBOL(dump_fpu);
EXPORT_SYMBOL(pte_alloc_one);
#ifndef CONFIG_SMP
EXPORT_SYMBOL(pgt_quicklists);
#endif
EXPORT_SYMBOL(put_fs_struct);

/* math-emu wants this */
EXPORT_SYMBOL(die_if_kernel);

/* Kernel thread creation. */
EXPORT_SYMBOL(kernel_thread);

/* prom symbols */
EXPORT_SYMBOL(idprom);
EXPORT_SYMBOL(prom_root_node);
EXPORT_SYMBOL(prom_getchild);
EXPORT_SYMBOL(prom_getsibling);
EXPORT_SYMBOL(prom_searchsiblings);
EXPORT_SYMBOL(prom_firstprop);
EXPORT_SYMBOL(prom_nextprop);
EXPORT_SYMBOL(prom_getproplen);
EXPORT_SYMBOL(prom_getproperty);
EXPORT_SYMBOL(prom_node_has_property);
EXPORT_SYMBOL(prom_setprop);
EXPORT_SYMBOL(saved_command_line);
EXPORT_SYMBOL(prom_getname);
EXPORT_SYMBOL(prom_finddevice);
EXPORT_SYMBOL(prom_feval);
EXPORT_SYMBOL(prom_getbool);
EXPORT_SYMBOL(prom_getstring);
EXPORT_SYMBOL(prom_getint);
EXPORT_SYMBOL(prom_getintdefault);
EXPORT_SYMBOL(__prom_getchild);
EXPORT_SYMBOL(__prom_getsibling);

/* sparc library symbols */
EXPORT_SYMBOL(__strlen);
EXPORT_SYMBOL(strlen);
EXPORT_SYMBOL(strnlen);
EXPORT_SYMBOL(__strlen_user);
EXPORT_SYMBOL(__strnlen_user);
EXPORT_SYMBOL(strcpy);
EXPORT_SYMBOL(strncpy);
EXPORT_SYMBOL(strcat);
EXPORT_SYMBOL(strncat);
EXPORT_SYMBOL(strcmp);
EXPORT_SYMBOL(strchr);
EXPORT_SYMBOL(strrchr);
EXPORT_SYMBOL(strpbrk);
EXPORT_SYMBOL(strtok);
EXPORT_SYMBOL(strstr);

#ifdef CONFIG_SOLARIS_EMUL_MODULE
EXPORT_SYMBOL(linux_sparc_syscall);
EXPORT_SYMBOL(rtrap);
EXPORT_SYMBOL(show_regs);
EXPORT_SYMBOL(solaris_syscall);
EXPORT_SYMBOL(syscall_trace);
EXPORT_SYMBOL(sunos_sys_table);
EXPORT_SYMBOL(sys_call_table32);
EXPORT_SYMBOL(tl0_solaris);
EXPORT_SYMBOL(sys_sigsuspend);
EXPORT_SYMBOL(sys_getppid);
EXPORT_SYMBOL(svr4_getcontext);
EXPORT_SYMBOL(svr4_setcontext);
EXPORT_SYMBOL(prom_cpu_nodes);
EXPORT_SYMBOL(sys_ioctl);
EXPORT_SYMBOL(sys32_ioctl);
EXPORT_SYMBOL(sparc32_open);
#endif

/* Special internal versions of library functions. */
EXPORT_SYMBOL(__memcpy);
EXPORT_SYMBOL(__memset);
EXPORT_SYMBOL(_clear_page);
EXPORT_SYMBOL(clear_user_page);
EXPORT_SYMBOL(copy_user_page);
EXPORT_SYMBOL(__bzero);
EXPORT_SYMBOL(__memscan_zero);
EXPORT_SYMBOL(__memscan_generic);
EXPORT_SYMBOL(__memcmp);
EXPORT_SYMBOL(__strncmp);
EXPORT_SYMBOL(__memmove);

EXPORT_SYMBOL(csum_partial_copy_sparc64);

/* Moving data to/from userspace. */
EXPORT_SYMBOL(__copy_to_user);
EXPORT_SYMBOL(__copy_from_user);
EXPORT_SYMBOL(__strncpy_from_user);
EXPORT_SYMBOL(__bzero_noasi);

/* Various address conversion macros use this. */
EXPORT_SYMBOL(phys_base);
EXPORT_SYMBOL(sparc64_valid_addr_bitmap);

/* No version information on this, heavily used in inline asm,
 * and will always be 'void __ret_efault(void)'.
 */
EXPORT_SYMBOL_NOVERS(__ret_efault);

/* No version information on these, as gcc produces such symbols. */
EXPORT_SYMBOL_NOVERS(memcmp);
EXPORT_SYMBOL_NOVERS(memcpy);
EXPORT_SYMBOL_NOVERS(memset);
EXPORT_SYMBOL_NOVERS(memmove);

void VISenter(void);
/* RAID code needs this */
EXPORT_SYMBOL_NOVERS(VISenter);

extern void batten_down_hatches(void);
/* for input/keybdev */
EXPORT_SYMBOL(batten_down_hatches);

#ifdef CONFIG_DEBUG_BUGVERBOSE
EXPORT_SYMBOL(do_BUG);
#endif

EXPORT_SYMBOL(tick_ops);
