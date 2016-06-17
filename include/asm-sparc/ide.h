/* $Id: ide.h,v 1.6 2000/05/27 00:49:37 davem Exp $
 * ide.h: SPARC PCI specific IDE glue.
 *
 * Copyright (C) 1997  David S. Miller (davem@caip.rutgers.edu)
 * Copyright (C) 1998  Eddie C. Dost   (ecd@skynet.be)
 * Adaptation from sparc64 version to sparc by Pete Zaitcev.
 */

#ifndef _SPARC_IDE_H
#define _SPARC_IDE_H

#ifdef __KERNEL__

#include <linux/config.h>
#include <asm/pgtable.h>
#include <asm/io.h>
#include <asm/hdreg.h>
#include <asm/psr.h>

#undef  MAX_HWIFS
#define MAX_HWIFS	2

static __inline__ int ide_default_irq(ide_ioreg_t base)
{
	return 0;
}

static __inline__ ide_ioreg_t ide_default_io_base(int index)
{
	return 0;
}

/*
 * Doing any sort of ioremap() here does not work
 * because this function may be called with null aguments.
 */
static __inline__ void ide_init_hwif_ports(hw_regs_t *hw, ide_ioreg_t data_port, ide_ioreg_t ctrl_port, int *irq)
{
	ide_ioreg_t reg =  data_port;
	int i;

	for (i = IDE_DATA_OFFSET; i <= IDE_STATUS_OFFSET; i++) {
		hw->io_ports[i] = reg;
		reg += 1;
	}
	if (ctrl_port) {
		hw->io_ports[IDE_CONTROL_OFFSET] = ctrl_port;
	} else {
		hw->io_ports[IDE_CONTROL_OFFSET] = 0;
	}
	if (irq != NULL)
		*irq = 0;
	hw->io_ports[IDE_IRQ_OFFSET] = 0;
}

/*
 * This registers the standard ports for this architecture with the IDE
 * driver.
 */
static __inline__ void ide_init_default_hwifs(void)
{
#ifndef CONFIG_BLK_DEV_IDEPCI
	hw_regs_t hw;
	int index;

	for (index = 0; index < MAX_HWIFS; index++) {
		ide_init_hwif_ports(&hw, ide_default_io_base(index), 0, NULL);
		hw.irq = ide_default_irq(ide_default_io_base(index));
		ide_register_hw(&hw, NULL);
	}
#endif /* CONFIG_BLK_DEV_IDEPCI */
}

#undef  SUPPORT_SLOW_DATA_PORTS
#define SUPPORT_SLOW_DATA_PORTS 0

#undef  SUPPORT_VLB_SYNC
#define SUPPORT_VLB_SYNC 0

#undef  HD_DATA
#define HD_DATA ((ide_ioreg_t)0)

/* From m68k code... */

#ifdef insl
#undef insl
#endif
#ifdef outsl
#undef outsl
#endif
#ifdef insw
#undef insw
#endif
#ifdef outsw
#undef outsw
#endif

#define insl(data_reg, buffer, wcount) insw(data_reg, buffer, (wcount)<<1)
#define outsl(data_reg, buffer, wcount) outsw(data_reg, buffer, (wcount)<<1)

#define insw(port, buf, nr) ide_insw((port), (buf), (nr))
#define outsw(port, buf, nr) ide_outsw((port), (buf), (nr))

static __inline__ void ide_insw(unsigned long port,
				void *dst,
				unsigned long count)
{
	volatile unsigned short *data_port;
	/* unsigned long end = (unsigned long)dst + (count << 1); */ /* P3 */
	u16 *ps = dst;
	u32 *pi;

	data_port = (volatile unsigned short *)port;

	if(((unsigned long)ps) & 0x2) {
		*ps++ = *data_port;
		count--;
	}
	pi = (u32 *)ps;
	while(count >= 2) {
		u32 w;

		w  = (*data_port) << 16;
		w |= (*data_port);
		*pi++ = w;
		count -= 2;
	}
	ps = (u16 *)pi;
	if(count)
		*ps++ = *data_port;

	/* __flush_dcache_range((unsigned long)dst, end); */ /* P3 see hme */
}

static __inline__ void ide_outsw(unsigned long port,
				 const void *src,
				 unsigned long count)
{
	volatile unsigned short *data_port;
	/* unsigned long end = (unsigned long)src + (count << 1); */
	const u16 *ps = src;
	const u32 *pi;

	data_port = (volatile unsigned short *)port;

	if(((unsigned long)src) & 0x2) {
		*data_port = *ps++;
		count--;
	}
	pi = (const u32 *)ps;
	while(count >= 2) {
		u32 w;

		w = *pi++;
		*data_port = (w >> 16);
		*data_port = w;
		count -= 2;
	}
	ps = (const u16 *)pi;
	if(count)
		*data_port = *ps;

	/* __flush_dcache_range((unsigned long)src, end); */ /* P3 see hme */
}

#endif /* __KERNEL__ */

#endif /* _SPARC_IDE_H */
