/*
 *  linux/arch/arm/kernel/armksyms.c
 *
 *  Copyright (C) 2000 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/user.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/in6.h>
#include <linux/interrupt.h>
#include <linux/pm.h>
#include <linux/vt_kern.h>

#include <asm/byteorder.h>
#include <asm/elf.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/pgalloc.h>
#include <asm/proc-fns.h>
#include <asm/processor.h>
#include <asm/semaphore.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/checksum.h>
#include <asm/mach-types.h>

extern void dump_thread(struct pt_regs *, struct user *);
extern int dump_fpu(struct pt_regs *, struct user_fp_struct *);
extern void inswb(unsigned int port, void *to, int len);
extern void outswb(unsigned int port, const void *to, int len);

extern void __bad_xchg(volatile void *ptr, int size);

/*
 * syscalls
 */
extern int sys_write(int, const char *, int);
extern int sys_read(int, char *, int);
extern int sys_lseek(int, off_t, int);
extern int sys_exit(int);

/*
 * libgcc functions - functions that are used internally by the
 * compiler...  (prototypes are not correct though, but that
 * doesn't really matter since they're not versioned).
 */
extern void __ashldi3(void);
extern void __ashrdi3(void);
extern void __divsi3(void);
extern void __lshrdi3(void);
extern void __modsi3(void);
extern void __muldi3(void);
extern void __ucmpdi2(void);
extern void __udivdi3(void);
extern void __umoddi3(void);
extern void __udivmoddi4(void);
extern void __udivsi3(void);
extern void __umodsi3(void);
extern void abort(void);

extern void ret_from_exception(void);
extern void fpundefinstr(void);
extern void fp_enter(void);

/*
 * This has a special calling convention; it doesn't
 * modify any of the usual registers, except for LR.
 */
extern void __do_softirq(void);

#define EXPORT_SYMBOL_ALIAS(sym,orig)		\
 const char __kstrtab_##sym[]			\
  __attribute__((section(".kstrtab"))) =	\
    __MODULE_STRING(sym);			\
 const struct module_symbol __ksymtab_##sym	\
  __attribute__((section("__ksymtab"))) =	\
    { (unsigned long)&orig, __kstrtab_##sym };

/*
 * floating point math emulator support.
 * These symbols will never change their calling convention...
 */
EXPORT_SYMBOL_ALIAS(kern_fp_enter,fp_enter);
EXPORT_SYMBOL_ALIAS(fp_printk,printk);
EXPORT_SYMBOL_ALIAS(fp_send_sig,send_sig);

#ifdef CONFIG_CPU_26
EXPORT_SYMBOL(fpundefinstr);
EXPORT_SYMBOL(ret_from_exception);
#endif

#ifdef CONFIG_VT
EXPORT_SYMBOL(kd_mksound);
#endif

EXPORT_SYMBOL_NOVERS(__do_softirq);

	/* platform dependent support */
EXPORT_SYMBOL(dump_thread);
EXPORT_SYMBOL(dump_fpu);
EXPORT_SYMBOL(__udelay);
EXPORT_SYMBOL(__ndelay);
EXPORT_SYMBOL(__const_delay);
#ifdef CONFIG_CPU_32
EXPORT_SYMBOL(__ioremap);
EXPORT_SYMBOL(__iounmap);
#endif
EXPORT_SYMBOL(kernel_thread);
EXPORT_SYMBOL(system_rev);
EXPORT_SYMBOL(system_serial_low);
EXPORT_SYMBOL(system_serial_high);
#ifdef CONFIG_DEBUG_BUGVERBOSE
EXPORT_SYMBOL(__bug);
#endif
EXPORT_SYMBOL(__bad_xchg);
EXPORT_SYMBOL(__readwrite_bug);
EXPORT_SYMBOL(enable_irq);
EXPORT_SYMBOL(disable_irq);
EXPORT_SYMBOL(pm_idle);
EXPORT_SYMBOL(pm_power_off);

	/* processor dependencies */
EXPORT_SYMBOL(__machine_arch_type);

	/* networking */
EXPORT_SYMBOL(csum_partial_copy_nocheck);
EXPORT_SYMBOL(__csum_ipv6_magic);

	/* io */
#ifndef __raw_readsb
EXPORT_SYMBOL_NOVERS(__raw_readsb);
#endif
#ifndef __raw_readsw
EXPORT_SYMBOL_NOVERS(__raw_readsw);
#endif
#ifndef __raw_readsl
EXPORT_SYMBOL_NOVERS(__raw_readsl);
#endif
#ifndef __raw_writesb
EXPORT_SYMBOL_NOVERS(__raw_writesb);
#endif
#ifndef __raw_writesw
EXPORT_SYMBOL_NOVERS(__raw_writesw);
#endif
#ifndef __raw_writesl
EXPORT_SYMBOL_NOVERS(__raw_writesl);
#endif

	/* address translation */
#ifndef __virt_to_phys__is_a_macro
EXPORT_SYMBOL(__virt_to_phys);
#endif
#ifndef __phys_to_virt__is_a_macro
EXPORT_SYMBOL(__phys_to_virt);
#endif
#ifndef __virt_to_bus__is_a_macro
EXPORT_SYMBOL(__virt_to_bus);
#endif
#ifndef __bus_to_virt__is_a_macro
EXPORT_SYMBOL(__bus_to_virt);
#endif

#ifndef CONFIG_NO_PGT_CACHE
EXPORT_SYMBOL(quicklists);
#endif

	/* string / mem functions */
EXPORT_SYMBOL_NOVERS(strcpy);
EXPORT_SYMBOL_NOVERS(strncpy);
EXPORT_SYMBOL_NOVERS(strcat);
EXPORT_SYMBOL_NOVERS(strncat);
EXPORT_SYMBOL_NOVERS(strcmp);
EXPORT_SYMBOL_NOVERS(strncmp);
EXPORT_SYMBOL_NOVERS(strchr);
EXPORT_SYMBOL_NOVERS(strlen);
EXPORT_SYMBOL_NOVERS(strnlen);
EXPORT_SYMBOL_NOVERS(strpbrk);
EXPORT_SYMBOL_NOVERS(strtok);
EXPORT_SYMBOL_NOVERS(strrchr);
EXPORT_SYMBOL_NOVERS(strstr);
EXPORT_SYMBOL_NOVERS(memset);
EXPORT_SYMBOL_NOVERS(memcpy);
EXPORT_SYMBOL_NOVERS(memmove);
EXPORT_SYMBOL_NOVERS(memcmp);
EXPORT_SYMBOL_NOVERS(memscan);
EXPORT_SYMBOL_NOVERS(memchr);
EXPORT_SYMBOL_NOVERS(__memzero);

	/* user mem (segment) */
#if defined(CONFIG_CPU_32)
EXPORT_SYMBOL(__arch_copy_from_user);
EXPORT_SYMBOL(__arch_copy_to_user);
EXPORT_SYMBOL(__arch_clear_user);
EXPORT_SYMBOL(__arch_strnlen_user);

	/* consistent area handling */
EXPORT_SYMBOL(pci_alloc_consistent);
EXPORT_SYMBOL(consistent_alloc);
EXPORT_SYMBOL(consistent_free);
EXPORT_SYMBOL(consistent_sync);

#elif defined(CONFIG_CPU_26)
EXPORT_SYMBOL(uaccess_kernel);
EXPORT_SYMBOL(uaccess_user);
#endif

EXPORT_SYMBOL_NOVERS(__get_user_1);
EXPORT_SYMBOL_NOVERS(__get_user_2);
EXPORT_SYMBOL_NOVERS(__get_user_4);
EXPORT_SYMBOL_NOVERS(__get_user_8);

EXPORT_SYMBOL_NOVERS(__put_user_1);
EXPORT_SYMBOL_NOVERS(__put_user_2);
EXPORT_SYMBOL_NOVERS(__put_user_4);
EXPORT_SYMBOL_NOVERS(__put_user_8);

	/* gcc lib functions */
EXPORT_SYMBOL_NOVERS(__ashldi3);
EXPORT_SYMBOL_NOVERS(__ashrdi3);
EXPORT_SYMBOL_NOVERS(__divsi3);
EXPORT_SYMBOL_NOVERS(__lshrdi3);
EXPORT_SYMBOL_NOVERS(__modsi3);
EXPORT_SYMBOL_NOVERS(__muldi3);
EXPORT_SYMBOL_NOVERS(__ucmpdi2);
EXPORT_SYMBOL_NOVERS(__udivdi3);
EXPORT_SYMBOL_NOVERS(__umoddi3);
EXPORT_SYMBOL_NOVERS(__udivmoddi4);
EXPORT_SYMBOL_NOVERS(__udivsi3);
EXPORT_SYMBOL_NOVERS(__umodsi3);
EXPORT_SYMBOL_NOVERS(abort);

	/* bitops */
EXPORT_SYMBOL(set_bit);
EXPORT_SYMBOL(test_and_set_bit);
EXPORT_SYMBOL(clear_bit);
EXPORT_SYMBOL(test_and_clear_bit);
EXPORT_SYMBOL(change_bit);
EXPORT_SYMBOL(test_and_change_bit);
EXPORT_SYMBOL(find_first_zero_bit);
EXPORT_SYMBOL(find_next_zero_bit);

	/* elf */
EXPORT_SYMBOL(elf_platform);
EXPORT_SYMBOL(elf_hwcap);

	/* syscalls */
EXPORT_SYMBOL(sys_write);
EXPORT_SYMBOL(sys_read);
EXPORT_SYMBOL(sys_lseek);
EXPORT_SYMBOL(sys_open);
EXPORT_SYMBOL(sys_exit);
EXPORT_SYMBOL(sys_wait4);

	/* semaphores */
EXPORT_SYMBOL_NOVERS(__down_failed);
EXPORT_SYMBOL_NOVERS(__down_interruptible_failed);
EXPORT_SYMBOL_NOVERS(__down_trylock_failed);
EXPORT_SYMBOL_NOVERS(__up_wakeup);

EXPORT_SYMBOL(get_wchan);
