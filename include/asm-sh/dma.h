#ifndef __ASM_SH_DMA_H
#define __ASM_SH_DMA_H

#include <linux/config.h>
#include <asm/io.h>		/* need byte IO */

#define MAX_DMA_CHANNELS 8
#define SH_MAX_DMA_CHANNELS 4

/* The maximum address that we can perform a DMA transfer to on this platform */
/* Don't define MAX_DMA_ADDRESS; it's useless on the SuperH and any
   occurrence should be flagged as an error.  */
/* But... */
/* XXX: This is not applicable to SuperH, just needed for alloc_bootmem */
#define MAX_DMA_ADDRESS      (PAGE_OFFSET+0x10000000)

#if defined(__sh3__)
#define SAR ((unsigned long[]){0xa4000020,0xa4000030,0xa4000040,0xa4000050})
#define DAR ((unsigned long[]){0xa4000024,0xa4000034,0xa4000044,0xa4000054})
#define DMATCR ((unsigned long[]){0xa4000028,0xa4000038,0xa4000048,0xa4000058})
#define CHCR ((unsigned long[]){0xa400002c,0xa400003c,0xa400004c,0xa400005c})
#define DMAOR 0xa4000060UL
#elif defined(__SH4__)
#define SAR ((unsigned long[]){0xbfa00000,0xbfa00010,0xbfa00020,0xbfa00030})
#define DAR ((unsigned long[]){0xbfa00004,0xbfa00014,0xbfa00024,0xbfa00034})
#define DMATCR ((unsigned long[]){0xbfa00008,0xbfa00018,0xbfa00028,0xbfa00038})
#define CHCR ((unsigned long[]){0xbfa0000c,0xbfa0001c,0xbfa0002c,0xbfa0003c})
#define DMAOR 0xbfa00040UL
#endif

#define DMTE_IRQ ((int[]){DMTE0_IRQ,DMTE1_IRQ,DMTE2_IRQ,DMTE3_IRQ})

#define DMA_MODE_READ	0x00	/* I/O to memory, no autoinit, increment, single mode */
#define DMA_MODE_WRITE	0x01	/* memory to I/O, no autoinit, increment, single mode */
#define DMA_AUTOINIT	0x10

#define REQ_L	0x00000000
#define REQ_E	0x00080000
#define RACK_H	0x00000000
#define RACK_L	0x00040000
#define ACK_R	0x00000000
#define ACK_W	0x00020000
#define ACK_H	0x00000000
#define ACK_L	0x00010000
#define DM_INC	0x00004000
#define DM_DEC	0x00008000
#define SM_INC	0x00001000
#define SM_DEC	0x00002000
#define RS_DUAL	0x00000000
#define RS_IN	0x00000200
#define RS_OUT	0x00000300
#define TM_BURST 0x0000080
#define TS_8	0x00000010
#define TS_16	0x00000020
#define TS_32	0x00000030
#define TS_64	0x00000000
#define TS_BLK	0x00000040
#define CHCR_DE 0x00000001
#define CHCR_TE 0x00000002
#define CHCR_IE 0x00000004

#define DMAOR_COD	0x00000008
#define DMAOR_AE	0x00000004
#define DMAOR_NMIF	0x00000002
#define DMAOR_DME	0x00000001

struct dma_info_t {
	unsigned int chan;
	unsigned int mode_read;
	unsigned int mode_write;
	unsigned long dev_addr;
	unsigned int mode;
	unsigned long mem_addr;
	unsigned int count;
};

static __inline__ void clear_dma_ff(unsigned int dmanr){}

/* These are in arch/sh/kernel/dma.c: */
extern unsigned long claim_dma_lock(void);
extern void release_dma_lock(unsigned long flags);
extern void setup_dma(unsigned int dmanr, struct dma_info_t *info);
extern void enable_dma(unsigned int dmanr);
extern void disable_dma(unsigned int dmanr);
extern void set_dma_mode(unsigned int dmanr, char mode);
extern void set_dma_addr(unsigned int dmanr, unsigned int a);
extern void set_dma_count(unsigned int dmanr, unsigned int count);
extern int get_dma_residue(unsigned int dmanr);

#ifdef CONFIG_PCI
extern int isa_dma_bridge_buggy;
#else
#define isa_dma_bridge_buggy 	(0)
#endif


#endif /* __ASM_SH_DMA_H */
