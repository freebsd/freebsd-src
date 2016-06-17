/*
 * linux/include/asm-arm/arch-shark/dma.h
 *
 * by Alexander Schulz
 */
#ifndef __ASM_ARCH_DMA_H
#define __ASM_ARCH_DMA_H

/* Use only the lowest 4MB, nothing else works.
 * The rest is not DMAable. See dev /  .properties
 * in OpenFirmware.
 */
#define MAX_DMA_ADDRESS		0xC0400000
#define MAX_DMA_CHANNELS	8
#define DMA_ISA_CASCADE         4

static inline void __arch_adjust_zones(int node, unsigned long *zone_size, unsigned long *zhole_size) 
{
  if (node != 0) return;
  /* Only the first 4 MB (=1024 Pages) are usable for DMA */
  zone_size[1] = zone_size[0] - 1024;
  zone_size[0] = 1024;
  zhole_size[1] = zhole_size[0];
  zhole_size[0] = 0;
}

#define arch_adjust_zones(node,size,holes) __arch_adjust_zones(node,size,holes)

#endif /* _ASM_ARCH_DMA_H */

