/*
 * linux/include/asm-arm/arch-sa1100/dma.h
 *
 * Generic SA1100 DMA support
 *
 * Copyright (C) 2000 Nicolas Pitre
 *
 */

#ifndef __ASM_ARCH_DMA_H
#define __ASM_ARCH_DMA_H

#include <linux/config.h>
#include "hardware.h"


/*
 * This is the maximum DMA address that can be DMAd to.
 */
#define MAX_DMA_ADDRESS		0xffffffff


/*
 * The regular generic DMA interface is inappropriate for the
 * SA1100 DMA model.  None of the SA1100 specific drivers using
 * DMA are portable anyway so it's pointless to try to twist the
 * regular DMA API to accommodate them.
 */
#define MAX_DMA_CHANNELS	0


/*
 * The SA1100 has six internal DMA channels.
 */
#define SA1100_DMA_CHANNELS     6


/*
 * The SA-1111 SAC has two DMA channels.
 */
#define SA1111_SAC_DMA_CHANNELS 2
#define SA1111_SAC_XMT_CHANNEL  0
#define SA1111_SAC_RCV_CHANNEL  1


/*
 * The SA-1111 SAC channels will reside in the same index space as
 * the built-in SA-1100 channels, and will take on the next available
 * identifiers after the 1100.
 */
#define SA1111_SAC_DMA_BASE     SA1100_DMA_CHANNELS

#ifdef CONFIG_SA1111
# define MAX_SA1100_DMA_CHANNELS (SA1100_DMA_CHANNELS + SA1111_SAC_DMA_CHANNELS)
#else
# define MAX_SA1100_DMA_CHANNELS SA1100_DMA_CHANNELS
#endif


/*
 * All possible SA1100 devices a DMA channel can be attached to.
 */
typedef enum {
	DMA_Ser0UDCWr  = DDAR_Ser0UDCWr,   /* Ser. port 0 UDC Write */
	DMA_Ser0UDCRd  = DDAR_Ser0UDCRd,   /* Ser. port 0 UDC Read */
	DMA_Ser1UARTWr = DDAR_Ser1UARTWr,  /* Ser. port 1 UART Write */
	DMA_Ser1UARTRd = DDAR_Ser1UARTRd,  /* Ser. port 1 UART Read */
	DMA_Ser1SDLCWr = DDAR_Ser1SDLCWr,  /* Ser. port 1 SDLC Write */
	DMA_Ser1SDLCRd = DDAR_Ser1SDLCRd,  /* Ser. port 1 SDLC Read */
	DMA_Ser2UARTWr = DDAR_Ser2UARTWr,  /* Ser. port 2 UART Write */
	DMA_Ser2UARTRd = DDAR_Ser2UARTRd,  /* Ser. port 2 UART Read */
	DMA_Ser2HSSPWr = DDAR_Ser2HSSPWr,  /* Ser. port 2 HSSP Write */
	DMA_Ser2HSSPRd = DDAR_Ser2HSSPRd,  /* Ser. port 2 HSSP Read */
	DMA_Ser3UARTWr = DDAR_Ser3UARTWr,  /* Ser. port 3 UART Write */
	DMA_Ser3UARTRd = DDAR_Ser3UARTRd,  /* Ser. port 3 UART Read */
	DMA_Ser4MCP0Wr = DDAR_Ser4MCP0Wr,  /* Ser. port 4 MCP 0 Write (audio) */
	DMA_Ser4MCP0Rd = DDAR_Ser4MCP0Rd,  /* Ser. port 4 MCP 0 Read (audio) */
	DMA_Ser4MCP1Wr = DDAR_Ser4MCP1Wr,  /* Ser. port 4 MCP 1 Write */
	DMA_Ser4MCP1Rd = DDAR_Ser4MCP1Rd,  /* Ser. port 4 MCP 1 Read */
	DMA_Ser4SSPWr  = DDAR_Ser4SSPWr,   /* Ser. port 4 SSP Write (16 bits) */
	DMA_Ser4SSPRd  = DDAR_Ser4SSPRd    /* Ser. port 4 SSP Read (16 bits) */
} dma_device_t;


typedef void (*dma_callback_t)( void *buf_id, int size );


/* SA1100 DMA API */
extern int sa1100_request_dma( dmach_t *channel, const char *device_id,
			       dma_device_t device );
extern int sa1100_dma_set_callback( dmach_t channel, dma_callback_t cb );
extern int sa1100_dma_set_spin( dmach_t channel, dma_addr_t addr, int size );
extern int sa1100_dma_queue_buffer( dmach_t channel, void *buf_id,
				    dma_addr_t data, int size );
extern int sa1100_dma_get_current( dmach_t channel, void **buf_id, dma_addr_t *addr );
extern int sa1100_dma_stop( dmach_t channel );
extern int sa1100_dma_resume( dmach_t channel );
extern int sa1100_dma_flush_all( dmach_t channel );
extern void sa1100_free_dma( dmach_t channel );
extern int sa1100_dma_sleep( dmach_t channel );
extern int sa1100_dma_wakeup( dmach_t channel );

/* Sa1111 DMA interface (all but registration uses the above) */
extern int sa1111_sac_request_dma( dmach_t *channel, const char *device_id,
				   unsigned int direction );
extern int sa1111_check_dma_bug( dma_addr_t addr );

#ifdef CONFIG_SA1111
static inline void
__arch_adjust_zones(int node, unsigned long *size, unsigned long *holes)
{
	unsigned int sz = 256;

	if (node != 0)
		sz = 0;

	size[1] = size[0] - sz;
	size[0] = sz;
}

#define arch_adjust_zones(node,size,holes) __arch_adjust_zones(node,size,holes)
#endif

#endif /* _ASM_ARCH_DMA_H */
