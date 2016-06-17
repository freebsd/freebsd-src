/*
 *  linux/include/asm-arm/param.h
 *
 *  Copyright (C) 1995-1999 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __ASM_PARAM_H
#define __ASM_PARAM_H

#include <asm/arch/param.h>	/* for HZ */
#include <asm/proc/page.h>	/* for EXEC_PAGE_SIZE */

#ifndef HZ
#define HZ 100
#endif
#if defined(__KERNEL__) && (HZ == 100)
#define hz_to_std(a) (a)
#endif

#ifndef NGROUPS
#define NGROUPS         32
#endif

#ifndef NOGROUP
#define NOGROUP         (-1)
#endif

/* max length of hostname */
#define MAXHOSTNAMELEN  64

#ifdef __KERNEL__
# define CLOCKS_PER_SEC	HZ
#endif

#endif

