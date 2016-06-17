/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * arch/sh64/kernel/sh_ksyms.c
 *
 * Copyright (C) 2000, 2001  Paolo Alberelli
 *
 */

#include <linux/config.h>


#if 0
#ifdef CONFIG_MODVERSIONS
#define EXPORT_SYMTAB 1
#endif
#endif

#include <linux/module.h>
#include <linux/smp.h>
#include <linux/user.h>
#include <linux/elfcore.h>
#include <linux/mca.h>
#include <linux/sched.h>
#include <linux/in6.h>
#include <linux/interrupt.h>
#include <linux/smp_lock.h>

#include <asm/semaphore.h>
#include <asm/processor.h>
#include <asm/uaccess.h>
#include <asm/checksum.h>
#include <asm/io.h>
#include <asm/hardirq.h>
#include <asm/delay.h>
#include <asm/irq.h>
#include <asm/pgtable.h>

extern void dump_thread(struct pt_regs *, struct user *);
extern int dump_fpu(elf_fpregset_t *);

#if 0
/* Not yet - there's no declaration of drive_info anywhere. */
#if defined(CONFIG_BLK_DEV_IDE) || defined(CONFIG_BLK_DEV_HD) || defined(CONFIG_BLK_DEV_IDE_MODULE) || defined(CONFIG_BLK_DEV_HD_MODULE)
extern struct drive_info_struct drive_info;
EXPORT_SYMBOL(drive_info);
#endif
#endif

/* platform dependent support */
EXPORT_SYMBOL(dump_thread);
EXPORT_SYMBOL(dump_fpu);
EXPORT_SYMBOL(iounmap);
EXPORT_SYMBOL(enable_irq);
EXPORT_SYMBOL(disable_irq);
EXPORT_SYMBOL(kernel_thread);

/* Networking helper routines. */
EXPORT_SYMBOL(csum_partial_copy);

EXPORT_SYMBOL(strtok);
EXPORT_SYMBOL(strpbrk);
EXPORT_SYMBOL(strstr);

#ifdef CONFIG_VT
EXPORT_SYMBOL(screen_info);
#endif

EXPORT_SYMBOL_NOVERS(__down);
EXPORT_SYMBOL_NOVERS(__down_trylock);
EXPORT_SYMBOL_NOVERS(__up);
EXPORT_SYMBOL_NOVERS(__put_user_asm_l);
EXPORT_SYMBOL_NOVERS(__get_user_asm_l);
EXPORT_SYMBOL_NOVERS(memcmp);
EXPORT_SYMBOL_NOVERS(memcpy);
EXPORT_SYMBOL_NOVERS(memset);
EXPORT_SYMBOL_NOVERS(memscan);
EXPORT_SYMBOL_NOVERS(strchr);
EXPORT_SYMBOL_NOVERS(strlen);

EXPORT_SYMBOL(flush_dcache_page);

/* Ugh.  These come in from libgcc.a at link time. */

extern void __sdivsi3(void);
extern void __muldi3(void);
extern void __udivsi3(void);
EXPORT_SYMBOL_NOVERS(__sdivsi3);
EXPORT_SYMBOL_NOVERS(__muldi3);
EXPORT_SYMBOL_NOVERS(__udivsi3);

