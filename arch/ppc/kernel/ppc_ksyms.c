#include <linux/config.h>
#include <linux/module.h>
#include <linux/threads.h>
#include <linux/smp.h>
#include <linux/elfcore.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/vt_kern.h>
#include <linux/nvram.h>
#include <linux/spinlock.h>
#include <linux/console.h>
#include <linux/irq.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/ide.h>

#include <asm/page.h>
#include <asm/semaphore.h>
#include <asm/processor.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/ide.h>
#include <asm/atomic.h>
#include <asm/bitops.h>
#include <asm/checksum.h>
#include <asm/pgtable.h>
#include <linux/adb.h>
#include <linux/cuda.h>
#include <linux/pmu.h>
#include <asm/prom.h>
#include <asm/system.h>
#include <asm/pci-bridge.h>
#include <asm/irq.h>
#include <asm/pmac_feature.h>
#include <asm/dma.h>
#include <asm/machdep.h>
#include <asm/hw_irq.h>
#include <asm/nvram.h>
#include <asm/mmu_context.h>
#include <asm/backlight.h>
#ifdef CONFIG_SMP
#include <asm/smplock.h>
#include <asm/smp.h>
#endif /* CONFIG_SMP */
#include <asm/time.h>
#include <asm/cputable.h>
#include <asm/btext.h>
#include <asm/div64.h>

#ifdef  CONFIG_8xx
#include <asm/commproc.h>
#endif

/* Tell string.h we don't want memcpy etc. as cpp defines */
#define EXPORT_SYMTAB_STROPS

extern void transfer_to_handler(void);
extern void syscall_trace(void);
extern void do_IRQ(struct pt_regs *regs);
extern void MachineCheckException(struct pt_regs *regs);
extern void AlignmentException(struct pt_regs *regs);
extern void ProgramCheckException(struct pt_regs *regs);
extern void SingleStepException(struct pt_regs *regs);
extern int do_signal(sigset_t *, struct pt_regs *);
extern int pmac_newworld;
extern int sys_sigreturn(struct pt_regs *regs);

long long __ashrdi3(long long, int);
long long __ashldi3(long long, int);
long long __lshrdi3(long long, int);
int abs(int);

extern unsigned char __res[];

extern unsigned long ret_to_user_hook;
extern unsigned long mm_ptov (unsigned long paddr);

EXPORT_SYMBOL(clear_page);
EXPORT_SYMBOL(do_signal);
EXPORT_SYMBOL(syscall_trace);
EXPORT_SYMBOL(transfer_to_handler);
EXPORT_SYMBOL(do_IRQ);
EXPORT_SYMBOL(MachineCheckException);
EXPORT_SYMBOL(AlignmentException);
EXPORT_SYMBOL(ProgramCheckException);
EXPORT_SYMBOL(SingleStepException);
EXPORT_SYMBOL(sys_sigreturn);
EXPORT_SYMBOL(ppc_n_lost_interrupts);
EXPORT_SYMBOL(ppc_lost_interrupts);
EXPORT_SYMBOL(enable_irq);
EXPORT_SYMBOL(disable_irq);
EXPORT_SYMBOL(disable_irq_nosync);
EXPORT_SYMBOL(probe_irq_mask);
#ifdef CONFIG_SMP
EXPORT_SYMBOL(kernel_flag);
#endif /* CONFIG_SMP */

EXPORT_SYMBOL(ISA_DMA_THRESHOLD);
EXPORT_SYMBOL_NOVERS(DMA_MODE_READ);
EXPORT_SYMBOL(DMA_MODE_WRITE);
#if defined(CONFIG_ALL_PPC)
EXPORT_SYMBOL(_prep_type);
EXPORT_SYMBOL(ucSystemType);
#endif

#if !__INLINE_BITOPS
EXPORT_SYMBOL(set_bit);
EXPORT_SYMBOL(clear_bit);
EXPORT_SYMBOL(change_bit);
EXPORT_SYMBOL(test_and_set_bit);
EXPORT_SYMBOL(test_and_clear_bit);
EXPORT_SYMBOL(test_and_change_bit);
#endif /* __INLINE_BITOPS */

EXPORT_SYMBOL(strcpy);
EXPORT_SYMBOL(strncpy);
EXPORT_SYMBOL(strcat);
EXPORT_SYMBOL(strncat);
EXPORT_SYMBOL(strchr);
EXPORT_SYMBOL(strrchr);
EXPORT_SYMBOL(strpbrk);
EXPORT_SYMBOL(strtok);
EXPORT_SYMBOL(strstr);
EXPORT_SYMBOL(strlen);
EXPORT_SYMBOL(strnlen);
EXPORT_SYMBOL(strcmp);
EXPORT_SYMBOL(strncmp);
EXPORT_SYMBOL(strcasecmp);
EXPORT_SYMBOL(__div64_32);

/* EXPORT_SYMBOL(csum_partial); already in net/netsyms.c */
EXPORT_SYMBOL(csum_partial_copy_generic);
EXPORT_SYMBOL(ip_fast_csum);
EXPORT_SYMBOL(csum_tcpudp_magic);

EXPORT_SYMBOL(__copy_tofrom_user);
EXPORT_SYMBOL(__clear_user);
EXPORT_SYMBOL(__strncpy_from_user);
EXPORT_SYMBOL(__strnlen_user);

/*
EXPORT_SYMBOL(inb);
EXPORT_SYMBOL(inw);
EXPORT_SYMBOL(inl);
EXPORT_SYMBOL(outb);
EXPORT_SYMBOL(outw);
EXPORT_SYMBOL(outl);
EXPORT_SYMBOL(outsl);*/

EXPORT_SYMBOL(_insb);
EXPORT_SYMBOL(_outsb);
EXPORT_SYMBOL(_insw);
EXPORT_SYMBOL(_outsw);
EXPORT_SYMBOL(_insl);
EXPORT_SYMBOL(_outsl);
EXPORT_SYMBOL(_insw_ns);
EXPORT_SYMBOL(_outsw_ns);
EXPORT_SYMBOL(_insl_ns);
EXPORT_SYMBOL(_outsl_ns);
EXPORT_SYMBOL(iopa);
EXPORT_SYMBOL(mm_ptov);
EXPORT_SYMBOL(vmalloc_start);
EXPORT_SYMBOL(ioremap_bot);
EXPORT_SYMBOL(ioremap);
#ifdef CONFIG_PTE_64BIT
EXPORT_SYMBOL(ioremap64);
#endif
EXPORT_SYMBOL(__ioremap);
EXPORT_SYMBOL(iounmap);

#if defined(CONFIG_BLK_DEV_IDE) || defined(CONFIG_BLK_DEV_IDE_MODULE) \
	|| defined(CONFIG_USB_STORAGE) || defined(CONFIG_USB_STORAGE_MODULE)
EXPORT_SYMBOL(ppc_ide_md);
#endif

#ifdef CONFIG_PCI
EXPORT_SYMBOL_NOVERS(isa_io_base);
EXPORT_SYMBOL_NOVERS(isa_mem_base);
EXPORT_SYMBOL_NOVERS(pci_dram_offset);
EXPORT_SYMBOL(pci_alloc_consistent);
EXPORT_SYMBOL(pci_free_consistent);
EXPORT_SYMBOL(pci_bus_io_base);
EXPORT_SYMBOL(pci_bus_io_base_phys);
EXPORT_SYMBOL(pci_bus_mem_base_phys);
EXPORT_SYMBOL(pci_bus_to_hose);
EXPORT_SYMBOL(pci_resource_to_bus);
EXPORT_SYMBOL(pci_phys_to_bus);
EXPORT_SYMBOL(pci_bus_to_phys);
#endif /* CONFIG_PCI */

#ifdef CONFIG_NOT_COHERENT_CACHE
EXPORT_SYMBOL(consistent_alloc);
EXPORT_SYMBOL(consistent_free);
EXPORT_SYMBOL(consistent_sync);
EXPORT_SYMBOL(consistent_sync_page);
EXPORT_SYMBOL(flush_dcache_all);
#endif

EXPORT_SYMBOL(start_thread);
EXPORT_SYMBOL(kernel_thread);

/*EXPORT_SYMBOL(__restore_flags);*/
/*EXPORT_SYMBOL(_disable_interrupts);
  EXPORT_SYMBOL(_enable_interrupts);*/
EXPORT_SYMBOL(flush_instruction_cache);
EXPORT_SYMBOL(giveup_fpu);
EXPORT_SYMBOL(enable_kernel_fp);
EXPORT_SYMBOL(flush_icache_range);
EXPORT_SYMBOL(flush_dcache_range);
EXPORT_SYMBOL(flush_icache_user_range);
EXPORT_SYMBOL(flush_icache_page);
EXPORT_SYMBOL(flush_dcache_page);
EXPORT_SYMBOL(xchg_u32);
#ifdef CONFIG_ALTIVEC
EXPORT_SYMBOL(last_task_used_altivec);
EXPORT_SYMBOL(giveup_altivec);
#endif /* CONFIG_ALTIVEC */
#ifdef CONFIG_SMP
EXPORT_SYMBOL(global_irq_lock);
EXPORT_SYMBOL(global_irq_count);
EXPORT_SYMBOL(global_irq_holder);
EXPORT_SYMBOL(__global_cli);
EXPORT_SYMBOL(__global_sti);
EXPORT_SYMBOL(__global_save_flags);
EXPORT_SYMBOL(__global_restore_flags);
#if SPINLOCK_DEBUG
EXPORT_SYMBOL(_spin_lock);
EXPORT_SYMBOL(_spin_unlock);
EXPORT_SYMBOL(spin_trylock);
EXPORT_SYMBOL(_read_lock);
EXPORT_SYMBOL(_read_unlock);
EXPORT_SYMBOL(_write_lock);
EXPORT_SYMBOL(_write_unlock);
#endif
EXPORT_SYMBOL(smp_call_function);
EXPORT_SYMBOL(smp_hw_index);
EXPORT_SYMBOL(smp_num_cpus);
EXPORT_SYMBOL(synchronize_irq);
#endif

EXPORT_SYMBOL(ppc_md);

#ifdef CONFIG_ADB
EXPORT_SYMBOL(adb_request);
EXPORT_SYMBOL(adb_register);
EXPORT_SYMBOL(adb_unregister);
EXPORT_SYMBOL(adb_poll);
EXPORT_SYMBOL(adb_try_handler_change);
#endif /* CONFIG_ADB */
#ifdef CONFIG_ADB_CUDA
EXPORT_SYMBOL(cuda_request);
EXPORT_SYMBOL(cuda_poll);
#endif /* CONFIG_ADB_CUDA */
#ifdef CONFIG_PMAC_BACKLIGHT
EXPORT_SYMBOL(get_backlight_level);
EXPORT_SYMBOL(set_backlight_level);
EXPORT_SYMBOL(set_backlight_enable);
EXPORT_SYMBOL(register_backlight_controller);
#endif /* CONFIG_PMAC_BACKLIGHT */
#if defined(CONFIG_ALL_PPC)
EXPORT_SYMBOL(_machine);
EXPORT_SYMBOL_NOVERS(sys_ctrler);
EXPORT_SYMBOL(find_devices);
EXPORT_SYMBOL(find_type_devices);
EXPORT_SYMBOL(find_compatible_devices);
EXPORT_SYMBOL(find_path_device);
EXPORT_SYMBOL(find_phandle);
EXPORT_SYMBOL(device_is_compatible);
EXPORT_SYMBOL(machine_is_compatible);
EXPORT_SYMBOL(find_all_nodes);
EXPORT_SYMBOL(get_property);
EXPORT_SYMBOL(request_OF_resource);
EXPORT_SYMBOL(release_OF_resource);
EXPORT_SYMBOL(pci_device_to_OF_node);
EXPORT_SYMBOL(pci_device_from_OF_node);
EXPORT_SYMBOL(pmac_newworld);
EXPORT_SYMBOL(nvram_read_byte);
EXPORT_SYMBOL(nvram_write_byte);
EXPORT_SYMBOL(pmac_xpram_read);
EXPORT_SYMBOL(pmac_xpram_write);
#endif /* defined(CONFIG_ALL_PPC) */
#if defined(CONFIG_BOOTX_TEXT)
EXPORT_SYMBOL(btext_update_display);
#endif
#if defined(CONFIG_SCSI) && defined(CONFIG_ALL_PPC)
EXPORT_SYMBOL(note_scsi_host);
#endif
#ifdef CONFIG_VT
EXPORT_SYMBOL(kd_mksound);
#endif
EXPORT_SYMBOL(to_tm);

EXPORT_SYMBOL_NOVERS(__ashrdi3);
EXPORT_SYMBOL_NOVERS(__ashldi3);
EXPORT_SYMBOL_NOVERS(__lshrdi3);
EXPORT_SYMBOL_NOVERS(memcpy);
EXPORT_SYMBOL_NOVERS(memset);
EXPORT_SYMBOL_NOVERS(memmove);
EXPORT_SYMBOL_NOVERS(memscan);
EXPORT_SYMBOL_NOVERS(memcmp);
EXPORT_SYMBOL_NOVERS(memchr);

EXPORT_SYMBOL(abs);

#if defined(CONFIG_VGA_CONSOLE) || defined(CONFIG_FB)
EXPORT_SYMBOL(screen_info);
#endif

EXPORT_SYMBOL(__delay);
EXPORT_SYMBOL(__sti);
EXPORT_SYMBOL(__sti_end);
EXPORT_SYMBOL(__cli);
EXPORT_SYMBOL(__cli_end);
EXPORT_SYMBOL(__save_flags_ptr);
EXPORT_SYMBOL(__save_flags_ptr_end);
EXPORT_SYMBOL(__restore_flags);
EXPORT_SYMBOL(__restore_flags_end);
EXPORT_SYMBOL(timer_interrupt_intercept);
EXPORT_SYMBOL(timer_interrupt);
EXPORT_SYMBOL(do_IRQ_intercept);
EXPORT_SYMBOL(irq_desc);
void ppc_irq_dispatch_handler(struct pt_regs *, int);
EXPORT_SYMBOL(ppc_irq_dispatch_handler);
EXPORT_SYMBOL(tb_ticks_per_jiffy);
EXPORT_SYMBOL(get_wchan);
EXPORT_SYMBOL(console_drivers);
#ifdef CONFIG_XMON
extern void xmon_printf(char *fmt, ...);
EXPORT_SYMBOL(xmon);
EXPORT_SYMBOL(xmon_printf);
#endif
EXPORT_SYMBOL(__up);
EXPORT_SYMBOL(__down);
EXPORT_SYMBOL(__down_interruptible);

#if defined(CONFIG_KGDB) || defined(CONFIG_XMON)
extern void (*debugger)(struct pt_regs *regs);
extern int (*debugger_bpt)(struct pt_regs *regs);
extern int (*debugger_sstep)(struct pt_regs *regs);
extern int (*debugger_iabr_match)(struct pt_regs *regs);
extern int (*debugger_dabr_match)(struct pt_regs *regs);
extern void (*debugger_fault_handler)(struct pt_regs *regs);

EXPORT_SYMBOL(debugger);
EXPORT_SYMBOL(debugger_bpt);
EXPORT_SYMBOL(debugger_sstep);
EXPORT_SYMBOL(debugger_iabr_match);
EXPORT_SYMBOL(debugger_dabr_match);
EXPORT_SYMBOL(debugger_fault_handler);
#endif

#if defined(CONFIG_8xx) || defined(CONFIG_4xx)
EXPORT_SYMBOL(__res);
#endif
#ifdef  CONFIG_8xx
EXPORT_SYMBOL(cpm_install_handler);
EXPORT_SYMBOL(cpm_free_handler);
EXPORT_SYMBOL(m8xx_cpm_hostalloc);
EXPORT_SYMBOL(m8xx_cpm_dpalloc);
#ifdef CONFIG_8xx_WDT
extern int m8xx_wdt_get_timeout(void);
extern void m8xx_wdt_reset(void);
EXPORT_SYMBOL(m8xx_wdt_get_timeout);
EXPORT_SYMBOL(m8xx_wdt_reset);
#endif
#endif /* CONFIG_8xx */

/* Those should really be inline */
EXPORT_SYMBOL(atomic_clear_mask);
EXPORT_SYMBOL(atomic_set_mask);

EXPORT_SYMBOL(ret_to_user_hook);
EXPORT_SYMBOL(next_mmu_context);
EXPORT_SYMBOL(set_context);
EXPORT_SYMBOL(handle_mm_fault); /* For MOL */
#ifdef CONFIG_SMP
extern int *hash_table_lock;
EXPORT_SYMBOL_NOVERS(hash_table_lock); /* For MOL */
#endif
EXPORT_SYMBOL_NOVERS(disarm_decr);
#ifdef CONFIG_PPC_STD_MMU
EXPORT_SYMBOL(flush_hash_page); /* For MOL */
extern long *intercept_table;
EXPORT_SYMBOL(intercept_table);
#endif
extern long *ret_from_intercept;
EXPORT_SYMBOL(ret_from_intercept);
EXPORT_SYMBOL(cur_cpu_spec);
#if defined(CONFIG_ALL_PPC)
extern unsigned long agp_special_page;
EXPORT_SYMBOL_NOVERS(agp_special_page);
#endif /* defined(CONFIG_ALL_PPC) */
