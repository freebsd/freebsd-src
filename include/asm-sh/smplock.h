#ifndef __ASM_SH_SMPLOCK_H
#define __ASM_SH_SMPLOCK_H

/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#include <linux/config.h>

#ifndef CONFIG_SMP

#define lock_kernel()				do { } while(0)
#define unlock_kernel()				do { } while(0)
#define release_kernel_lock(task, cpu, depth)	((depth) = 1)
#define reacquire_kernel_lock(task, cpu, depth)	do { } while(0)

#else
#error "We do not support SMP on SH"
#endif /* CONFIG_SMP */

#endif /* __ASM_SH_SMPLOCK_H */
