/*
 *  linux/include/asm-arm/ide.h
 *
 *  Copyright (C) 1994-1996  Linus Torvalds & authors
 */

/*
 *  This file contains the i386 architecture specific IDE code.
 */

#ifndef __ASMARM_IDE_H
#define __ASMARM_IDE_H

#ifdef __KERNEL__

#ifndef MAX_HWIFS
#define MAX_HWIFS	4
#endif

#include <asm/arch/ide.h>

/*
 * We always use the new IDE port registering,
 * so these are fixed here.
 */
#define ide_default_io_base(i)		((ide_ioreg_t)0)
#define ide_default_irq(b)		(0)

#include <asm-generic/ide_iops.h>

#endif /* __KERNEL__ */

#endif /* __ASMARM_IDE_H */
