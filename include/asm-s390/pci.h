#ifndef __ASM_S390_PCI_H
#define __ASM_S390_PCI_H

/* S/390 systems don't have a PCI bus. This file is just here because some stupid .c code
 * includes it even if CONFIG_PCI is not set.
 */

/* The PCI address space does equal the physical memory
 * address space.  The networking and block device layers use
 * this boolean for bounce buffer decisions.
 */
#define PCI_DMA_BUS_IS_PHYS     (1)

#endif /* __ASM_S390_PCI_H */

