/*
 *  arch/s390/kernel/s390_ksyms.c
 *
 *  S390 version
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/smp.h>
#include <asm/checksum.h>
#include <asm/delay.h>
#include <asm/setup.h>
#include <asm/softirq.h>
#if CONFIG_IP_MULTICAST
#include <net/arp.h>
#endif

/*
 * memory management
 */
EXPORT_SYMBOL_NOVERS(_oi_bitmap);
EXPORT_SYMBOL_NOVERS(_ni_bitmap);
EXPORT_SYMBOL_NOVERS(_zb_findmap);
EXPORT_SYMBOL_NOVERS(__copy_from_user_asm);
EXPORT_SYMBOL_NOVERS(__copy_to_user_asm);
EXPORT_SYMBOL_NOVERS(__clear_user_asm);
EXPORT_SYMBOL(diag10);

/*
 * semaphore ops
 */
EXPORT_SYMBOL(__up);
EXPORT_SYMBOL(__down);
EXPORT_SYMBOL(__down_interruptible);
EXPORT_SYMBOL(__down_trylock);

/*
 * string functions
 */
EXPORT_SYMBOL_NOVERS(memcmp);
EXPORT_SYMBOL_NOVERS(memset);
EXPORT_SYMBOL_NOVERS(memmove);
EXPORT_SYMBOL_NOVERS(memscan);
EXPORT_SYMBOL_NOVERS(strlen);
EXPORT_SYMBOL_NOVERS(strchr);
EXPORT_SYMBOL_NOVERS(strcmp);
EXPORT_SYMBOL_NOVERS(strncat);
EXPORT_SYMBOL_NOVERS(strncmp);
EXPORT_SYMBOL_NOVERS(strncpy);
EXPORT_SYMBOL_NOVERS(strnlen);
EXPORT_SYMBOL_NOVERS(strrchr);
EXPORT_SYMBOL_NOVERS(strstr);
EXPORT_SYMBOL_NOVERS(strtok);
EXPORT_SYMBOL_NOVERS(strpbrk);

/*
 * misc.
 */
EXPORT_SYMBOL(machine_flags);
EXPORT_SYMBOL(__udelay);
EXPORT_SYMBOL(kernel_thread);
EXPORT_SYMBOL(csum_fold);
EXPORT_SYMBOL(console_mode);
EXPORT_SYMBOL(console_device);
EXPORT_SYMBOL(pfix_get_addr);
EXPORT_SYMBOL(pfix_get_page_addr);
EXPORT_SYMBOL(get_storage_key);
EXPORT_SYMBOL_NOVERS(do_call_softirq);
EXPORT_SYMBOL(sys_wait4);
