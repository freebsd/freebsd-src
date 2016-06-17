#include <linux/module.h>
#include <linux/linkage.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/user.h>
#include <linux/elfcore.h>
#include <linux/in6.h>
#include <linux/interrupt.h>
#include <linux/config.h>

#include <asm/setup.h>
#include <asm/machdep.h>
#include <asm/pgalloc.h>
#include <asm/pgtable.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/semaphore.h>
#include <asm/checksum.h>
#include <asm/hardirq.h>
#include <asm/softirq.h>
#include <asm/rtc.h>
#include <asm/hwtest.h>

asmlinkage long long __ashldi3 (long long, int);
asmlinkage long long __ashrdi3 (long long, int);
asmlinkage long long __lshrdi3 (long long, int);
asmlinkage long long __muldi3 (long long, long long);
extern char m68k_debug_device[];

extern void dump_thread(struct pt_regs *, struct user *);
extern int dump_fpu (struct pt_regs *regs, struct user_m68kfp_struct *fpu);

/* platform dependent support */

EXPORT_SYMBOL(m68k_machtype);
EXPORT_SYMBOL(m68k_cputype);
EXPORT_SYMBOL(m68k_is040or060);
EXPORT_SYMBOL(m68k_realnum_memory);
EXPORT_SYMBOL(m68k_memory);
#ifndef CONFIG_SUN3
EXPORT_SYMBOL(cache_push);
EXPORT_SYMBOL(cache_clear);
#ifndef CONFIG_SINGLE_MEMORY_CHUNK
EXPORT_SYMBOL(mm_vtop);
EXPORT_SYMBOL(mm_ptov);
EXPORT_SYMBOL(mm_end_of_chunk);
#endif /* !CONFIG_SINGLE_MEMORY_CHUNK */
EXPORT_SYMBOL(__ioremap);
EXPORT_SYMBOL(iounmap);
EXPORT_SYMBOL(kernel_set_cachemode);
#ifndef mm_cachebits
EXPORT_SYMBOL(mm_cachebits);
#endif
#endif /* !CONFIG_SUN3 */
EXPORT_SYMBOL(m68k_debug_device);
EXPORT_SYMBOL(mach_hwclk);
EXPORT_SYMBOL(mach_get_ss);
EXPORT_SYMBOL(mach_get_rtc_pll);
EXPORT_SYMBOL(mach_set_rtc_pll);
EXPORT_SYMBOL(dump_fpu);
EXPORT_SYMBOL(dump_thread);
EXPORT_SYMBOL(strnlen);
EXPORT_SYMBOL(strrchr);
EXPORT_SYMBOL(strstr);
EXPORT_SYMBOL(strtok);
EXPORT_SYMBOL(enable_irq);
EXPORT_SYMBOL(disable_irq);
EXPORT_SYMBOL(kernel_thread);
#ifdef CONFIG_VME
EXPORT_SYMBOL(vme_brdtype);
#endif
EXPORT_SYMBOL(hwreg_present);
EXPORT_SYMBOL(hwreg_write);

/* Networking helper routines. */
EXPORT_SYMBOL(csum_partial_copy);

/* The following are special because they're not called
   explicitly (the C compiler generates them).  Fortunately,
   their interface isn't gonna change any time soon now, so
   it's OK to leave it out of version control.  */
EXPORT_SYMBOL_NOVERS(__ashldi3);
EXPORT_SYMBOL_NOVERS(__ashrdi3);
EXPORT_SYMBOL_NOVERS(__lshrdi3);
EXPORT_SYMBOL_NOVERS(memcpy);
EXPORT_SYMBOL_NOVERS(memset);
EXPORT_SYMBOL_NOVERS(memcmp);
EXPORT_SYMBOL_NOVERS(memscan);
EXPORT_SYMBOL_NOVERS(__muldi3);

EXPORT_SYMBOL_NOVERS(__down_failed);
EXPORT_SYMBOL_NOVERS(__down_failed_interruptible);
EXPORT_SYMBOL_NOVERS(__down_failed_trylock);
EXPORT_SYMBOL_NOVERS(__up_wakeup);

EXPORT_SYMBOL(get_wchan);
