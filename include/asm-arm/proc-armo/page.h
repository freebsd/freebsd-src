/*
 *  linux/include/asm-arm/proc-armo/page.h
 *
 *  Copyright (C) 1995, 1996 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __ASM_PROC_PAGE_H
#define __ASM_PROC_PAGE_H

#include <linux/config.h>

/* PAGE_SHIFT determines the page size.  This is configurable. */
#if defined(CONFIG_PAGESIZE_16)
#define PAGE_SHIFT	14		/* 16K */
#else		/* default */
#define PAGE_SHIFT	15		/* 32K */
#endif

#define EXEC_PAGESIZE   32768

#endif /* __ASM_PROC_PAGE_H */
