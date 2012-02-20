/*-
 * Copyright (c) 2011
 *	Ben Gray <ben.r.gray@gmail.com>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/**
 * DMA device driver interface for the TI OMAP SoC
 *
 * See the omap_dma.c file for implementation details.
 *
 * Reference:
 *  OMAP35x Applications Processor
 *   Technical Reference Manual
 *  (omap35xx_techref.pdf)
 */
#ifndef _OMAP_DMA_H_
#define _OMAP_DMA_H_

#define OMAP_SDMA_ENDIAN_BIG          0x1
#define OMAP_SDMA_ENDIAN_LITTLE       0x0

#define OMAP_SDMA_BURST_NONE          0x0
#define OMAP_SDMA_BURST_16            0x1
#define OMAP_SDMA_BURST_32            0x2
#define OMAP_SDMA_BURST_64            0x3

#define OMAP_SDMA_DATA_8BITS_SCALAR   0x0
#define OMAP_SDMA_DATA_16BITS_SCALAR  0x1
#define OMAP_SDMA_DATA_32BITS_SCALAR  0x2

#define OMAP_SDMA_ADDR_CONSTANT       0x0
#define OMAP_SDMA_ADDR_POST_INCREMENT 0x1
#define OMAP_SDMA_ADDR_SINGLE_INDEX   0x2
#define OMAP_SDMA_ADDR_DOUBLE_INDEX   0x3

/**
 * Status flags for the DMA callback
 *
 */
#define OMAP_SDMA_STATUS_DROP                  (1UL << 1)
#define OMAP_SDMA_STATUS_HALF                  (1UL << 2)
#define OMAP_SDMA_STATUS_FRAME                 (1UL << 3)
#define OMAP_SDMA_STATUS_LAST                  (1UL << 4)
#define OMAP_SDMA_STATUS_BLOCK                 (1UL << 5)
#define OMAP_SDMA_STATUS_SYNC                  (1UL << 6)
#define OMAP_SDMA_STATUS_PKT                   (1UL << 7)
#define OMAP_SDMA_STATUS_TRANS_ERR             (1UL << 8)
#define OMAP_SDMA_STATUS_SECURE_ERR            (1UL << 9)
#define OMAP_SDMA_STATUS_SUPERVISOR_ERR        (1UL << 10)
#define OMAP_SDMA_STATUS_MISALIGNED_ADRS_ERR   (1UL << 11)
#define OMAP_SDMA_STATUS_DRAIN_END             (1UL << 12)

#define OMAP_SDMA_SYNC_FRAME                   (1UL << 0)
#define OMAP_SDMA_SYNC_BLOCK                   (1UL << 1)
#define OMAP_SDMA_SYNC_PACKET                  (OMAP_SDMA_SYNC_FRAME | OMAP_SDMA_SYNC_BLOCK)
#define OMAP_SDMA_SYNC_TRIG_ON_SRC             (1UL << 8)
#define OMAP_SDMA_SYNC_TRIG_ON_DST             (1UL << 9)

#define OMAP_SDMA_IRQ_FLAG_DROP                (1UL << 1)
#define OMAP_SDMA_IRQ_FLAG_HALF_FRAME_COMPL    (1UL << 2)
#define OMAP_SDMA_IRQ_FLAG_FRAME_COMPL         (1UL << 3)
#define OMAP_SDMA_IRQ_FLAG_START_LAST_FRAME    (1UL << 4)
#define OMAP_SDMA_IRQ_FLAG_BLOCK_COMPL         (1UL << 5)
#define OMAP_SDMA_IRQ_FLAG_ENDOF_PKT           (1UL << 7)
#define OMAP_SDMA_IRQ_FLAG_DRAIN               (1UL << 12)

int omap_dma_activate_channel(unsigned int *ch,
    void (*callback)(unsigned int ch, uint32_t status, void *data), void *data);
int omap_dma_deactivate_channel(unsigned int ch);
int omap_dma_start_xfer(unsigned int ch, unsigned int src_paddr,
    unsigned long dst_paddr, unsigned int frmcnt, unsigned int elmcnt);
int omap_dma_start_xfer_packet(unsigned int ch, unsigned int src_paddr,
    unsigned long dst_paddr, unsigned int frmcnt, unsigned int elmcnt, 
    unsigned int pktsize);
int omap_dma_stop_xfer(unsigned int ch);
int omap_dma_enable_channel_irq(unsigned int ch, uint32_t flags);
int omap_dma_disable_channel_irq(unsigned int ch);
int omap_dma_get_channel_status(unsigned int ch, uint32_t *status);
int omap_dma_set_xfer_endianess(unsigned int ch, unsigned int src, unsigned int dst);
int omap_dma_set_xfer_burst(unsigned int ch, unsigned int src, unsigned int dst);
int omap_dma_set_xfer_data_type(unsigned int ch, unsigned int type);
int omap_dma_set_callback(unsigned int ch,
    void (*callback)(unsigned int ch, uint32_t status, void *data), void *data);
int omap_dma_sync_params(unsigned int ch, unsigned int trigger, unsigned int mode);
int omap_dma_set_addr_mode(unsigned int ch, unsigned int src_mode, unsigned int dst_mode);

#endif /* _OMAP_DMA_H_ */
