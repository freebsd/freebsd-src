/*
 * parport.h: SuperH-specific parport initialisation
 *
 * This file should only be included by drivers/parport/parport_pc.c.
 *
 */

#ifndef _ASM_SH_PARPORT_H
#define _ASM_SH_PARPORT_H

#include <asm/machvec.h>

static int __devinit parport_pc_find_nonpci_ports (int autoirq, int autodma)
{
	if (MACH_HS7729PCI)
		return !!parport_pc_probe_port(0x378, 0, 5, 
					       PARPORT_DMA_NONE, NULL);

	return 0;
}

#endif /* _ASM_SH_PARPORT_H */
