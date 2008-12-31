/*-
 * Copyright (c) 2003 Dag-Erling Coïdan Smørgrav
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/dev/sound/pci/au88x0.h,v 1.2.28.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef _AU88X0_H_INCLUDED
#define _AU88X0_H_INCLUDED

/*
 * Chipset parameters
 */
struct au88x0_chipset {
	const char		*auc_name;
	uint32_t		 auc_pci_id;

	/* General control register */
	uint32_t		 auc_control;
#define	AU88X0_CTL_MIDI_ENABLE		0x0001
#define	AU88X0_CTL_GAME_ENABLE		0x0008
#define	AU88X0_CTL_IRQ_ENABLE		0x4000

	/* IRQ control register */
	uint32_t		 auc_irq_source;
#define	AU88X0_IRQ_FATAL_ERR		0x0001
#define	AU88X0_IRQ_PARITY_ERR		0x0002
#define	AU88X0_IRQ_REG_ERR		0x0004
#define	AU88X0_IRQ_FIFO_ERR		0x0008
#define	AU88X0_IRQ_DMA_ERR		0x0010
#define	AU88X0_IRQ_PCMOUT		0x0020
#define	AU88X0_IRQ_TIMER		0x1000
#define	AU88X0_IRQ_MIDI			0x2000
#define	AU88X0_IRQ_MODEM		0x4000
	uint32_t		 auc_irq_mask;
	uint32_t		 auc_irq_control;
#define		AU88X0_IRQ_PENDING_BIT		0x0001
	uint32_t		 auc_irq_status;

	/* DMA control registers */
	uint32_t		 auc_dma_control;

	/* FIFOs */
	int			 auc_fifo_size;
	int			 auc_wt_fifos;
	uint32_t		 auc_wt_fifo_base;
	uint32_t		 auc_wt_fifo_ctl;
	uint32_t		 auc_wt_dma_ctl;
	int			 auc_adb_fifos;
	uint32_t		 auc_adb_fifo_base;
	uint32_t		 auc_adb_fifo_ctl;
	uint32_t		 auc_adb_dma_ctl;

	/* Routing */
	uint32_t		 auc_adb_route_base;
	int			 auc_adb_route_bits;
	int			 auc_adb_codec_in;
	int			 auc_adb_codec_out;
};

/*
 * Channel information
 */
struct au88x0_chan_info {
	struct au88x0_info	*auci_aui;
	struct pcm_channel	*auci_pcmchan;
	struct snd_dbuf		*auci_buf;
	int			 auci_dir;
};

/*
 * Device information
 */
struct au88x0_info {
	/* the device we're associated with */
	device_t		 aui_dev;
	uint32_t		 aui_model;
	struct au88x0_chipset	*aui_chipset;

	/* parameters */
	bus_size_t		 aui_bufsize;
	int			 aui_wt_fifos;
	int			 aui_wt_fifo_ctl;
	int			 aui_adb_fifos;
	int			 aui_adb_fifo_ctl;
	int			 aui_fifo_size;
	uint32_t		 aui_chanbase;

	/* bus_space tag and handle */
	bus_space_tag_t		 aui_spct;
	bus_space_handle_t	 aui_spch;

	/* register space */
	int			 aui_regtype;
	int			 aui_regid;
	struct resource		*aui_reg;

	/* irq */
	int			 aui_irqtype;
	int			 aui_irqid;
	struct resource		*aui_irq;
	void			*aui_irqh;

	/* dma */
	bus_dma_tag_t		 aui_dmat;

	/* codec */
	struct ac97_info	*aui_ac97i;

	/* channels */
	struct au88x0_chan_info	 aui_chan[2];
};

/*
 * Common parameters
 */
#define AU88X0_SETTLE_DELAY	1000
#define AU88X0_RETRY_COUNT	10
#define AU88X0_BUFSIZE_MIN	0x1000
#define AU88X0_BUFSIZE_DFLT	0x4000
#define AU88X0_BUFSIZE_MAX	0x4000

/*
 * Codec control registers
 *
 * AU88X0_CODEC_CHANNEL	array of 32 32-bit words
 *
 * AU88X0_CODEC_CONTROL	control register
 *
 *    bit     16	ready
 *
 * AU88X0_CODEC_IO	I/O register
 *
 *    bits  0-15	contents of codec register
 *    bits 16-22	address of codec register
 *    bit     23	0 for read, 1 for write
 */
#define AU88X0_CODEC_CHANNEL	0x29080
#define AU88X0_CODEC_CONTROL	0x29184
#define		AU88X0_CDCTL_WROK	0x00000100
#define AU88X0_CODEC_IO		0x29188
#define		AU88X0_CDIO_DATA_SHIFT	0
#define		AU88X0_CDIO_DATA_MASK	0x0000ffff
#define		AU88X0_CDIO_ADDR_SHIFT	16
#define		AU88X0_CDIO_ADDR_MASK	0x007f0000
#define		AU88X0_CDIO_RDBIT	0x00000000
#define		AU88X0_CDIO_WRBIT	0x00800000
#define AU88X0_CDIO_READ(a) (AU88X0_CDIO_RDBIT | \
	 (((a) << AU88X0_CDIO_ADDR_SHIFT) & AU88X0_CDIO_ADDR_MASK))
#define AU88X0_CDIO_WRITE(a, d) (AU88X0_CDIO_WRBIT | \
	 (((a) << AU88X0_CDIO_ADDR_SHIFT) & AU88X0_CDIO_ADDR_MASK) | \
	 (((d) << AU88X0_CDIO_DATA_SHIFT) & AU88X0_CDIO_DATA_MASK))
#define AU88X0_CODEC_ENABLE	0x29190

#endif
