/*
 * Architecture-specific kernel symbols
 */
#include <linux/config.h>
#include <linux/module.h>

#include <linux/pm.h>
EXPORT_SYMBOL(pm_idle);

#include <linux/string.h>

EXPORT_SYMBOL_NOVERS(memset);
EXPORT_SYMBOL(memchr);
EXPORT_SYMBOL(memcmp);
EXPORT_SYMBOL_NOVERS(memcpy);
EXPORT_SYMBOL(memmove);
EXPORT_SYMBOL(memscan);
EXPORT_SYMBOL(strcat);
EXPORT_SYMBOL(strchr);
EXPORT_SYMBOL(strcmp);
EXPORT_SYMBOL(strcpy);
EXPORT_SYMBOL(strlen);
EXPORT_SYMBOL(strncat);
EXPORT_SYMBOL(strncmp);
EXPORT_SYMBOL(strncpy);
EXPORT_SYMBOL(strnlen);
EXPORT_SYMBOL(strrchr);
EXPORT_SYMBOL(strstr);
EXPORT_SYMBOL(strtok);
EXPORT_SYMBOL(strpbrk);

#include <linux/irq.h>
EXPORT_SYMBOL(isa_irq_to_vector_map);
EXPORT_SYMBOL(enable_irq);
EXPORT_SYMBOL(disable_irq);
EXPORT_SYMBOL(disable_irq_nosync);

#include <linux/interrupt.h>
EXPORT_SYMBOL(probe_irq_mask);

#include <linux/in6.h>
#include <asm/checksum.h>
/* not coded yet?? EXPORT_SYMBOL(csum_ipv6_magic); */
EXPORT_SYMBOL(csum_partial_copy_nocheck);
EXPORT_SYMBOL(csum_tcpudp_magic);
EXPORT_SYMBOL(ip_compute_csum);
EXPORT_SYMBOL(ip_fast_csum);

#include <asm/io.h>
EXPORT_SYMBOL(__ia64_memcpy_fromio);
EXPORT_SYMBOL(__ia64_memcpy_toio);
EXPORT_SYMBOL(__ia64_memset_c_io);
EXPORT_SYMBOL(io_space);

#include <asm/semaphore.h>
EXPORT_SYMBOL_NOVERS(__down);
EXPORT_SYMBOL_NOVERS(__down_interruptible);
EXPORT_SYMBOL_NOVERS(__down_trylock);
EXPORT_SYMBOL_NOVERS(__up);

#include <asm/page.h>
EXPORT_SYMBOL(clear_page);

#include <asm/pgtable.h>
EXPORT_SYMBOL(vmalloc_end);
EXPORT_SYMBOL(ia64_page_valid);

#include <asm/processor.h>
# ifndef CONFIG_NUMA
EXPORT_SYMBOL(_cpu_data);
# endif
EXPORT_SYMBOL(kernel_thread);

#include <asm/system.h>
#ifdef CONFIG_IA64_DEBUG_IRQ
EXPORT_SYMBOL(last_cli_ip);
#endif

#include <asm/pgalloc.h>

EXPORT_SYMBOL(flush_tlb_range);

#ifdef CONFIG_SMP

EXPORT_SYMBOL(smp_flush_tlb_all);

#include <asm/current.h>
#include <asm/hardirq.h>
EXPORT_SYMBOL(synchronize_irq);

#include <asm/smp.h>
EXPORT_SYMBOL(smp_call_function);
EXPORT_SYMBOL(smp_call_function_single);
EXPORT_SYMBOL(cpu_online_map);
EXPORT_SYMBOL(ia64_cpu_to_sapicid);

#include <linux/smp.h>
EXPORT_SYMBOL(smp_num_cpus);

#include <asm/smplock.h>
EXPORT_SYMBOL(kernel_flag);

/* #include <asm/system.h> */
EXPORT_SYMBOL(__global_sti);
EXPORT_SYMBOL(__global_cli);
EXPORT_SYMBOL(__global_save_flags);
EXPORT_SYMBOL(__global_restore_flags);

#else /* !CONFIG_SMP */

EXPORT_SYMBOL(local_flush_tlb_all);

#endif /* !CONFIG_SMP */

#include <asm/uaccess.h>
EXPORT_SYMBOL(__copy_user);
EXPORT_SYMBOL(__do_clear_user);
EXPORT_SYMBOL(__strlen_user);
EXPORT_SYMBOL(__strncpy_from_user);
EXPORT_SYMBOL(__strnlen_user);

#include <asm/unistd.h>
EXPORT_SYMBOL(__ia64_syscall);

/* from arch/ia64/lib */
extern void __divsi3(void);
extern void __udivsi3(void);
extern void __modsi3(void);
extern void __umodsi3(void);
extern void __divdi3(void);
extern void __udivdi3(void);
extern void __moddi3(void);
extern void __umoddi3(void);

EXPORT_SYMBOL_NOVERS(__divsi3);
EXPORT_SYMBOL_NOVERS(__udivsi3);
EXPORT_SYMBOL_NOVERS(__modsi3);
EXPORT_SYMBOL_NOVERS(__umodsi3);
EXPORT_SYMBOL_NOVERS(__divdi3);
EXPORT_SYMBOL_NOVERS(__udivdi3);
EXPORT_SYMBOL_NOVERS(__moddi3);
EXPORT_SYMBOL_NOVERS(__umoddi3);

extern unsigned long ia64_iobase;
EXPORT_SYMBOL(ia64_iobase);

#include <asm/pal.h>
EXPORT_SYMBOL(ia64_pal_call_phys_stacked);
EXPORT_SYMBOL(ia64_pal_call_phys_static);
EXPORT_SYMBOL(ia64_pal_call_stacked);
EXPORT_SYMBOL(ia64_pal_call_static);
EXPORT_SYMBOL(ia64_load_scratch_fpregs);
EXPORT_SYMBOL(ia64_save_scratch_fpregs);

extern struct efi efi;
EXPORT_SYMBOL(efi);

#include <linux/proc_fs.h>
extern struct proc_dir_entry *efi_dir;
EXPORT_SYMBOL(efi_dir);

#include <asm/machvec.h>
#ifdef CONFIG_IA64_GENERIC
EXPORT_SYMBOL(ia64_mv);
#endif
EXPORT_SYMBOL(machvec_noop);
#ifdef CONFIG_PERFMON
#include <asm/perfmon.h>
EXPORT_SYMBOL(pfm_install_alternate_syswide_subsystem);
EXPORT_SYMBOL(pfm_remove_alternate_syswide_subsystem);
#endif

#ifdef CONFIG_ACPI
#include <linux/acpi.h>
extern acpi_status acpi_hp_csr_space(acpi_handle, u64 *, u64 *);
EXPORT_SYMBOL(acpi_hp_csr_space);
#endif
