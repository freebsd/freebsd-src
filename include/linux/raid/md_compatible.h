
/*
   md.h : Multiple Devices driver compatibility layer for Linux 2.0/2.2
          Copyright (C) 1998 Ingo Molnar
	  
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.
   
   You should have received a copy of the GNU General Public License
   (for example /usr/src/linux/COPYING); if not, write to the Free
   Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  
*/

#include <linux/version.h>

#ifndef _MD_COMPATIBLE_H
#define _MD_COMPATIBLE_H

/** 2.3/2.4 stuff: **/

#include <linux/reboot.h>
#include <linux/vmalloc.h>
#include <linux/blkpg.h>

/* 000 */
#define md__get_free_pages(x,y) __get_free_pages(x,y)

#if defined(__i386__) || defined(__x86_64__)
/* 001 */
static __inline__ int md_cpu_has_mmx(void)
{
	return test_bit(X86_FEATURE_MMX,  &boot_cpu_data.x86_capability);
}
#else
#define md_cpu_has_mmx()       (0)
#endif

/* 002 */
#define md_clear_page(page)        clear_page(page)

/* 003 */
#define MD_EXPORT_SYMBOL(x) EXPORT_SYMBOL(x)

/* 004 */
#define md_copy_to_user(x,y,z) copy_to_user(x,y,z)

/* 005 */
#define md_copy_from_user(x,y,z) copy_from_user(x,y,z)

/* 006 */
#define md_put_user put_user

/* 007 */
static inline int md_capable_admin(void)
{
	return capable(CAP_SYS_ADMIN);
}

/* 008 */
#define MD_FILE_TO_INODE(file) ((file)->f_dentry->d_inode)

/* 009 */
static inline void md_flush_signals (void)
{
	spin_lock(&current->sigmask_lock);
	flush_signals(current);
	spin_unlock(&current->sigmask_lock);
}
 
/* 010 */
static inline void md_init_signals (void)
{
        current->exit_signal = SIGCHLD;
        siginitsetinv(&current->blocked, sigmask(SIGKILL));
}

/* 011 */
#define md_signal_pending signal_pending

/* 012 - md_set_global_readahead - nowhere used */

/* 013 */
#define md_mdelay(x) mdelay(x)

/* 014 */
#define MD_SYS_DOWN SYS_DOWN
#define MD_SYS_HALT SYS_HALT
#define MD_SYS_POWER_OFF SYS_POWER_OFF

/* 015 */
#define md_register_reboot_notifier register_reboot_notifier

/* 016 */
#define md_test_and_set_bit test_and_set_bit

/* 017 */
#define md_test_and_clear_bit test_and_clear_bit

/* 018 */
#define md_atomic_set atomic_set

/* 019 */
#define md_lock_kernel lock_kernel
#define md_unlock_kernel unlock_kernel

/* 020 */

#include <linux/init.h>

#define md__init __init
#define md__initdata __initdata
#define md__initfunc(__arginit) __initfunc(__arginit)

/* 021 */


/* 022 */

#define md_list_head list_head
#define MD_LIST_HEAD(name) LIST_HEAD(name)
#define MD_INIT_LIST_HEAD(ptr) INIT_LIST_HEAD(ptr)
#define md_list_add list_add
#define md_list_del list_del
#define md_list_empty list_empty

#define md_list_entry(ptr, type, member) list_entry(ptr, type, member)

/* 023 */

#define md_schedule_timeout schedule_timeout

/* 024 */
#define md_need_resched(tsk) ((tsk)->need_resched)

/* 025 */
#define md_spinlock_t spinlock_t
#define MD_SPIN_LOCK_UNLOCKED SPIN_LOCK_UNLOCKED

#define md_spin_lock spin_lock
#define md_spin_unlock spin_unlock
#define md_spin_lock_irq spin_lock_irq
#define md_spin_unlock_irq spin_unlock_irq
#define md_spin_unlock_irqrestore spin_unlock_irqrestore
#define md_spin_lock_irqsave spin_lock_irqsave

/* 026 */
typedef wait_queue_head_t md_wait_queue_head_t;
#define MD_DECLARE_WAITQUEUE(w,t) DECLARE_WAITQUEUE((w),(t))
#define MD_DECLARE_WAIT_QUEUE_HEAD(x) DECLARE_WAIT_QUEUE_HEAD(x)
#define md_init_waitqueue_head init_waitqueue_head

/* END */

#endif 

