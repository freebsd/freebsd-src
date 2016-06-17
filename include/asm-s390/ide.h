/*
 *  linux/include/asm-arm/ide.h
 *
 *  Copyright (C) 1994-1996  Linus Torvalds & authors
 */

/* s390 does not have IDE */

#ifndef __ASMS390_IDE_H
#define __ASMS390_IDE_H

#ifdef __KERNEL__

#ifndef MAX_HWIFS
#define MAX_HWIFS	0
#endif

/*
 * We always use the new IDE port registering,
 * so these are fixed here.
 */
#define ide_default_io_base(i)		((ide_ioreg_t)0)
#define ide_default_irq(b)		(0)

#endif /* __KERNEL__ */

#endif /* __ASMARM_IDE_H */
