#ifndef _LINUX_THREADS_H
#define _LINUX_THREADS_H

#include <linux/config.h>

/*
 * The default limit for the nr of threads is now in
 * /proc/sys/kernel/threads-max.
 */
 
#ifdef CONFIG_SMP
#define NR_CPUS	CONFIG_NR_CPUS
#else
#define NR_CPUS	1
#endif

#define MIN_THREADS_LEFT_FOR_ROOT 4

/*
 * This controls the maximum pid allocated to a process
 */
#define PID_MAX 0x8000

#endif
