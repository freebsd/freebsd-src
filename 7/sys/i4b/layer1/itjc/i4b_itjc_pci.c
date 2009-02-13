/*-
 *   Copyright (c) 2000, 2001 Sergio Prallon. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *   3. Neither the name of the author nor the names of any co-contributors
 *      may be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *   4. Altered versions must be plainly marked as such, and must not be
 *      misrepresented as being the original software and/or documentation.
 *   
 *   THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 *   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *   ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 *   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 *   OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 *   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 *   SUCH DAMAGE.
 */

/*---------------------------------------------------------------------------
 *
 *	i4b_itjc_pci.c: NetJet-S hardware driver
 *	----------------------------------------
 *      last edit-date: [Sat May 13 15:25:47 2006]
 *
 *---------------------------------------------------------------------------*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_i4b.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/mbuf.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <sys/socket.h>
#include <net/if.h>

#include <i4b/include/i4b_debug.h>
#include <i4b/include/i4b_ioctl.h>
#include <i4b/include/i4b_trace.h>

#include <i4b/include/i4b_global.h>
#include <i4b/include/i4b_mbuf.h>

#include <i4b/layer1/i4b_l1.h>

#include <i4b/layer1/itjc/i4b_hdlc.h>	/* XXXXXXXXXXXXXXXXXXXXXXXX */

#include <i4b/layer1/isic/i4b_isic.h>
#include <i4b/layer1/isic/i4b_isac.h>

#include <i4b/layer1/itjc/i4b_itjc_ext.h>

#define PCI_TJNET_VID (0xe159)
#define PCI_TJ300_DID (0x0001)


/*
 * Function prototypes
 */

static int  itjc_probe(device_t dev);
static int  itjc_attach(device_t dev);
static void itjc_shutdown(device_t dev);
static void itjc_intr(void *xsc);
static int  itjc_dma_start(struct l1_softc *sc);
static void itjc_dma_stop(struct l1_softc *sc);
static void itjc_isac_intr(struct l1_softc *sc);
static void itjc_init_linktab(struct l1_softc *sc);
static void itjc_bchannel_setup(int unit, int h_chan, int bprot, 
	int activate);
static void itjc_bchannel_stat(int unit, int h_chan, bchan_statistics_t *bsp);


/*
 * Shorter names to bus resource manager routines.
 */

#define itjc_bus_setup(sc)						\
	bus_space_handle_t h =						\
		rman_get_bushandle((sc)->sc_resources.io_base[0]);	\
	bus_space_tag_t    t =						\
		rman_get_bustag((sc)->sc_resources.io_base[0]);

#define itjc_read_1(port)	(bus_space_read_1(t, h, (port)))
#define itjc_read_4(port)	(bus_space_read_4(t, h, (port)))
#define itjc_write_1(port, data) (bus_space_write_1(t, h, (port), (data)))
#define itjc_write_4(port, data) (bus_space_write_4(t, h, (port), (data)))
#define itjc_read_multi_1(port, buf, size)				\
	(bus_space_read_multi_1(t, h, (port), (buf), (size)))
#define itjc_write_multi_1(port, buf, size)				\
	(bus_space_write_multi_1(t, h, (port), (buf), (size)))


/*---------------------------------------------------------------------------*
 *	Glue data to register ourselves as a PCI device driver.
 *---------------------------------------------------------------------------*/

static device_method_t itjc_pci_methods[] =
{
	/* Device interface */
	DEVMETHOD(device_probe,		itjc_probe),
	DEVMETHOD(device_attach,	itjc_attach),
	DEVMETHOD(device_shutdown,	itjc_shutdown),

	/* bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),

	{ 0, 0 }
};

static driver_t itjc_pci_driver =
{
	"itjc",
	itjc_pci_methods,
	sizeof(struct l1_softc)
};

static devclass_t itjc_pci_devclass;

DRIVER_MODULE(netjet, pci, itjc_pci_driver, itjc_pci_devclass, 0, 0);

/*
 * Jump table for multiplex routines.
 */

struct i4b_l1mux_func itjc_l1mux_func =
{
	itjc_ret_linktab,
	itjc_set_linktab,
	itjc_mph_command_req,
	itjc_ph_data_req,
	itjc_ph_activate_req,
};

struct l1_softc *itjc_scp[ITJC_MAXUNIT];


/*---------------------------------------------------------------------------*
 *	Tiger300/320 PCI ASIC registers.
 *---------------------------------------------------------------------------*/

/*
 *	Register offsets from i/o base.
 */
enum tiger_regs
{
	TIGER_RESET_PIB_CL_TIME	= 0x00,
	TIGER_DMA_OPER		= 0x01,
	TIGER_AUX_PORT_CNTL	= 0x02,
	TIGER_AUX_PORT_DATA	= 0x03,
	TIGER_INT0_MASK		= 0x04,
	TIGER_INT1_MASK		= 0x05,
	TIGER_INT0_STATUS	= 0x06,
	TIGER_INT1_STATUS	= 0x07,
	TIGER_DMA_WR_START_ADDR	= 0x08,
	TIGER_DMA_WR_INT_ADDR	= 0x0C,
	TIGER_DMA_WR_END_ADDR	= 0x10,
	TIGER_DMA_WR_CURR_ADDR	= 0x14,
	TIGER_DMA_RD_START_ADDR	= 0x18,
	TIGER_DMA_RD_INT_ADDR	= 0x1C,
	TIGER_DMA_RD_END_ADDR	= 0x20,
	TIGER_DMA_RD_CURR_ADDR	= 0x24,
	TIGER_PULSE_COUNTER	= 0x28,
};

/*
 * Bits on the above registers.
 */

enum tiger_reg_bits
{
/* Reset and PIB Cycle Timing */

	TIGER_DMA_OP_MODE_MASK		= 0x80,
		TIGER_SELF_ADDR_DMA	= 0x00,	/* Wrap around ending addr */
		TIGER_NORMAL_DMA	= 0x80,	/* Stop at ending addr */

	TIGER_DMA_INT_MODE_MASK		= 0x40,
		TIGER_DONT_LATCH_DMA_INT= 0x00,	/* Bits on int0 status will be
						   set only while curr addr
						   equals int or end addr */
		TIGER_LATCH_DMA_INT	= 0x40,	/* Bits on int0 status remain
						   set until cleared by CPU */

	TIGER_PIB_CYCLE_TIMING_MASK	= 0x30,
		TIGER_PIB_3_CYCLES	= 0x00,
		TIGER_PIB_5_CYCLES	= 0x10,
		TIGER_PIB_12_CYCLES	= 0x20,

	TIGER_RESET_MASK		= 0x0F,
		TIGER_RESET_PULSE_COUNT	= 0x08,
		TIGER_RESET_SERIAL_PORT	= 0x04,
		TIGER_RESET_DMA_LOGIC	= 0x02,
		TIGER_RESET_EXTERNAL	= 0x01,
		TIGER_RESET_ALL		= 0x0F,
	
/* DMA Operation */
	TIGER_DMA_RESTART_MASK		= 0x02,
		TIGER_HOLD_DMA		= 0x00,
		TIGER_RESTART_DMA	= 0x02,

	TIGER_DMA_ENABLE_MASK		= 0x01,
		TIGER_ENABLE_DMA	= 0x01,
		TIGER_DISABLE_DMA	= 0x00,

/* AUX Port Control & Data plus Interrupt 1 Mask & Status  */
	TIGER_AUX_7_MASK		= 0x80,
	TIGER_AUX_6_MASK		= 0x40,
	TIGER_AUX_5_MASK		= 0x20,
	TIGER_AUX_4_MASK		= 0x10,
	TIGER_ISAC_INT_MASK		= 0x10,
	TIGER_AUX_3_MASK		= 0x08,
	TIGER_AUX_2_MASK		= 0x04,
	TIGER_AUX_1_MASK		= 0x02,
	TIGER_AUX_0_MASK		= 0x01,

/* AUX Port Control */
		TIGER_AUX_7_IS_INPUT	= 0x00,
		TIGER_AUX_7_IS_OUTPUT	= 0x80,
		TIGER_AUX_6_IS_INPUT	= 0x00,
		TIGER_AUX_6_IS_OUTPUT	= 0x40,
		TIGER_AUX_5_IS_INPUT	= 0x00,
		TIGER_AUX_5_IS_OUTPUT	= 0x20,
		TIGER_AUX_4_IS_INPUT	= 0x00,
		TIGER_AUX_4_IS_OUTPUT	= 0x10,
		TIGER_AUX_3_IS_INPUT	= 0x00,
		TIGER_AUX_3_IS_OUTPUT	= 0x08,
		TIGER_AUX_2_IS_INPUT	= 0x00,
		TIGER_AUX_2_IS_OUTPUT	= 0x04,
		TIGER_AUX_1_IS_INPUT	= 0x00,
		TIGER_AUX_1_IS_OUTPUT	= 0x02,
		TIGER_AUX_0_IS_INPUT	= 0x00,
		TIGER_AUX_0_IS_OUTPUT	= 0x01,
		TIGER_AUX_NJ_DEFAULT	= 0xEF, /* All but ISAC int is output */

/* Interrupt 0 Mask & Status */
	TIGER_PCI_TARGET_ABORT_INT_MASK	= 0x20,
		TIGER_NO_TGT_ABORT_INT	= 0x00,
		TIGER_TARGET_ABORT_INT	= 0x20,
	TIGER_PCI_MASTER_ABORT_INT_MASK	= 0x10,
		TIGER_NO_MST_ABORT_INT	= 0x00,
		TIGER_MASTER_ABORT_INT	= 0x10,
	TIGER_DMA_RD_END_INT_MASK	= 0x08,
		TIGER_NO_RD_END_INT	= 0x00,
		TIGER_RD_END_INT	= 0x08,
	TIGER_DMA_RD_INT_INT_MASK	= 0x04,
		TIGER_NO_RD_INT_INT	= 0x00,
		TIGER_RD_INT_INT	= 0x04,
	TIGER_DMA_WR_END_INT_MASK	= 0x02,
		TIGER_NO_WR_END_INT	= 0x00,
		TIGER_WR_END_INT	= 0x02,
	TIGER_DMA_WR_INT_INT_MASK	= 0x01,
		TIGER_NO_WR_INT_INT	= 0x00,
		TIGER_WR_INT_INT	= 0x01,

/* Interrupt 1 Mask & Status */
		TIGER_NO_AUX_7_INT	= 0x00,
		TIGER_AUX_7_INT		= 0x80,
		TIGER_NO_AUX_6_INT	= 0x00,
		TIGER_AUX_6_INT		= 0x40,
		TIGER_NO_AUX_5_INT	= 0x00,
		TIGER_AUX_5_INT		= 0x20,
		TIGER_NO_AUX_4_INT	= 0x00,
		TIGER_AUX_4_INT		= 0x10,
		TIGER_NO_ISAC_INT	= 0x00,
		TIGER_ISAC_INT		= 0x10,
		TIGER_NO_AUX_3_INT	= 0x00,
		TIGER_AUX_3_INT		= 0x08,
		TIGER_NO_AUX_2_INT	= 0x00,
		TIGER_AUX_2_INT		= 0x04,
		TIGER_NO_AUX_1_INT	= 0x00,
		TIGER_AUX_1_INT		= 0x02,
		TIGER_NO_AUX_0_INT	= 0x00,
		TIGER_AUX_0_INT		= 0x01
};

/*
 * Peripheral Interface Bus definitions. This is an ISA like bus
 * created by the Tiger ASIC to keep ISA chips like the ISAC happy
 * on a PCI environment.
 *
 * Since the PIB only supplies 4 addressing lines, the 2 higher bits
 * (A4 & A5) of the ISAC register addresses are wired on the 2 lower
 * AUX lines. Another restriction is that all I/O to the PIB (8bit
 * wide) is mapped on the PCI side as 32bit data. So the PCI address
 * of a given ISAC register has to be multiplied by 4 before being
 * added to the PIB base offset.
 */
enum tiger_pib_regs_defs
{
	/* Offset from the I/O base to the ISAC registers. */
	PIB_OFFSET		= 0xC0,
	PIB_LO_ADDR_MASK	= 0x0F,		
	PIB_HI_ADDR_MASK	= 0x30,
	PIB_LO_ADDR_SHIFT	= 2,	/* Align on dword boundary */
	PIB_HI_ADDR_SHIFT	= 4	/* Right shift to AUX_1 & AUX_0 */
};


#define	itjc_set_pib_addr_msb(a)					\
(									\
	itjc_write_1(TIGER_AUX_PORT_DATA,				\
		((a) & PIB_HI_ADDR_MASK) >> PIB_HI_ADDR_SHIFT)		\
)

#define	itjc_pib_2_pci(a)						\
(									\
	(((a) & PIB_LO_ADDR_MASK) << PIB_LO_ADDR_SHIFT) + PIB_OFFSET	\
)

#define itjc_get_dma_offset(ctx,reg)					\
(									\
	(u_int16_t)((bus_addr_t)itjc_read_4((reg)) - (ctx)->bus_addr)	\
)


/*
 * IOM-2 serial channel 0 DMA data ring buffers.
 *
 * The Tiger300/320 ASIC do not nothing more than transfer via DMA the
 * first 32 bits of every IOM-2 frame on the serial interface to the
 * ISAC. So we have no framing/deframing facilities like we would have
 * with an HSCX, having to do the job with CPU cycles. On the plus side
 * we are able to specify large rings which can limit the occurrence of
 * over/underruns.
 */

enum
{
	ITJC_RING_SLOT_WORDS	= 64,
	ITJC_RING_WORDS		= 3 * ITJC_RING_SLOT_WORDS,
	ITJC_RING_SLOT_BYTES	= 4 * ITJC_RING_SLOT_WORDS,
	ITJC_RING_BYTES		= 4 * ITJC_RING_WORDS,
	ITJC_DMA_POOL_WORDS	= 2 * ITJC_RING_WORDS,
	ITJC_DMA_POOL_BYTES	= 4 * ITJC_DMA_POOL_WORDS
};

#define	itjc_ring_add(x, d)	(((x) + 4 * (d)) % ITJC_RING_BYTES)
#define itjc_ring_sub(x, d)	(((x) + ITJC_RING_BYTES - 4 * (d))	\
					% ITJC_RING_BYTES)


enum
{
	TIGER_CH_A		= 0,
	TIGER_CH_B		= 1,

	HSCX_CH_A		= 0,	/* For compatibility reasons. */
	HSCX_CH_B		= 1,
};

enum
{
	ITJC_TEL_SILENCE_BYTE	= 0x00,
	ITJC_HDLC_FLAG_BYTE	= 0x7E,
	ITJC_HDLC_ABORT_BYTE	= 0xFF
};

/*
 * Hardware DMA control block (one per card).
 */
typedef enum
{
	ITJC_DS_LOAD_FAILED	= -1,
	ITJC_DS_FREE		=  0,
	ITJC_DS_LOADING,
	ITJC_DS_STOPPED,
	ITJC_DS_RUNNING
}
	dma_state_t;

typedef struct
{
	dma_state_t	state;
	u_int8_t	*pool;
	bus_addr_t	bus_addr;
	bus_dma_tag_t	tag;
	bus_dmamap_t	map;
	int		error;
}
	dma_context_t;

dma_context_t
	dma_context	[ ITJC_MAXUNIT ];

/*
 * B-channel DMA control blocks (4 per card -- 1 RX & 1 TX per channel).
 */
typedef enum
{
	ITJC_RS_IDLE	= 0,
	ITJC_RS_ACTIVE
}
	dma_rx_state_t;

typedef enum
{
	ITJC_TS_IDLE	= 0,
	ITJC_TS_ACTIVE,
	ITJC_TS_AFTER_XDU
}
	dma_tx_state_t;

typedef struct
{
	u_int8_t	*ring;
	bus_addr_t	bus_addr;
	u_int16_t	next_read;
	u_int16_t	hdlc_len;
	u_int16_t	hdlc_tmp;
	u_int16_t	hdlc_crc;
	u_int16_t	hdlc_ib;
	u_int8_t	hdlc_blevel;
	u_int8_t	hdlc_flag;
	dma_rx_state_t	state;
}
	dma_rx_context_t;

typedef struct
{
	u_int8_t	*ring;
	bus_addr_t	bus_addr;
	u_int16_t	next_write;
	u_int32_t	hdlc_tmp;
	u_int16_t	hdlc_blevel;
	u_int16_t	hdlc_crc;
	u_int16_t	hdlc_ib;
	u_int16_t	next_frame;
	u_int16_t	filled;
	u_int8_t	hdlc_flag;
	dma_tx_state_t	state;
}
	dma_tx_context_t;

dma_rx_context_t
	dma_rx_context	[ ITJC_MAXUNIT ] [ 2 ];

dma_tx_context_t
	dma_tx_context	[ ITJC_MAXUNIT ] [ 2 ];

/*
 * Used by the mbuf handling functions.
 */
typedef enum
{
	ITJC_MB_CURR = 0,
	ITJC_MB_NEXT = 1,
	ITJC_MB_NEW  = 2
}
	which_mb_t;


/*---------------------------------------------------------------------------*
 *	itjc_map_callback - get DMA bus address from resource mgr.
 *---------------------------------------------------------------------------*/
static void
itjc_map_callback(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	dma_context_t		*ctx = (dma_context_t *)arg;

	if (error)
	{
		ctx->error = error;
		ctx->state = ITJC_DS_LOAD_FAILED;
		return;
	}

        ctx->bus_addr = segs->ds_addr;
	ctx->state = ITJC_DS_STOPPED;
}


/*---------------------------------------------------------------------------*
 *	itjc_dma_start - Complete DMA setup & start the Tiger DMA engine.
 *---------------------------------------------------------------------------*/
static int
itjc_dma_start(struct l1_softc *sc)
{
	int			unit	= sc->sc_unit;
	dma_context_t		*ctx	= &dma_context[unit];
	dma_rx_context_t	*rxc	= &dma_rx_context[unit][0];
	dma_tx_context_t	*txc	= &dma_tx_context[unit][0];
	bus_addr_t		ba;
	u_int8_t		i;
	u_int32_t		*pool_end,
				*ip;

	itjc_bus_setup(sc);

	/* See if it is already running. */

	if (ctx->state == ITJC_DS_RUNNING)
		return 0;

	if (ctx->state == ITJC_DS_LOAD_FAILED)
	{
		NDBGL1(L1_ERROR, "itjc%d: dma_start: DMA map loading "
			"failed (error=%d).\n", unit, ctx->error);
		return 1;
	}

	if (ctx->state != ITJC_DS_STOPPED)
	{
		NDBGL1(L1_ERROR, "itjc%d: dma_start: Unexpected DMA "
			"state (%d).\n", unit, ctx->state);
		return 1;
	}

	/*
	 * Initialize the DMA control structures (hardware & B-channel).
	 */
	ba = ctx->bus_addr;

	txc->ring = ctx->pool + TIGER_CH_A;
	rxc->ring = ctx->pool + TIGER_CH_A + ITJC_RING_BYTES;

	txc->bus_addr = ba;
	rxc->bus_addr = ba + ITJC_RING_BYTES;

	++rxc; ++txc;

	txc->ring = ctx->pool + TIGER_CH_B;
	rxc->ring = ctx->pool + TIGER_CH_B + ITJC_RING_BYTES;

	txc->bus_addr = ba;
	rxc->bus_addr = ba + ITJC_RING_BYTES;

	/*
	 * Fill the DMA ring buffers with IOM-2 channel 0 frames made of
	 * idle/abort sequences for the B & D channels and NOP for IOM-2
	 * cmd/ind, monitor handshake & data.
	 */
	pool_end = (u_int32_t *)ctx->pool + ITJC_DMA_POOL_WORDS;
	for (ip = (u_int32_t *)ctx->pool; ip < pool_end; ++ip)
		*ip = 0xFFFFFFFF;

	/*
	 * Program the Tiger DMA gears.
	 */

	itjc_write_4(TIGER_DMA_WR_START_ADDR, ba);
	itjc_write_4(TIGER_DMA_WR_INT_ADDR, ba + ITJC_RING_SLOT_BYTES - 4);
	itjc_write_4(TIGER_DMA_WR_END_ADDR, ba + ITJC_RING_BYTES - 4);

	ba += ITJC_RING_BYTES;

	itjc_write_4(TIGER_DMA_RD_START_ADDR, ba);
	itjc_write_4(TIGER_DMA_RD_INT_ADDR, ba + ITJC_RING_SLOT_BYTES * 2 - 4);
	itjc_write_4(TIGER_DMA_RD_END_ADDR, ba + ITJC_RING_BYTES - 4);

	itjc_write_1(TIGER_INT0_MASK, 
		TIGER_WR_END_INT | TIGER_WR_INT_INT | TIGER_RD_INT_INT);

	itjc_write_1(TIGER_DMA_OPER, TIGER_ENABLE_DMA);

	/*
	 * See if it really started.
	 */
	ba = itjc_read_4(TIGER_DMA_RD_CURR_ADDR);
	for (i = 0; i < 10; ++i)
	{
		DELAY(SEC_DELAY/1000);
		if (ba != itjc_read_4(TIGER_DMA_RD_CURR_ADDR))
		{
			ctx->state = ITJC_DS_RUNNING;
			return 0;
		}
	}

	NDBGL1(L1_ERROR, "itjc%d: dma_start: DMA start failed.\n ", unit);
	return 1;
}


/*---------------------------------------------------------------------------*
 *	itjc_dma_stop - Stop the Tiger DMA engine.
 *---------------------------------------------------------------------------*/
static void
itjc_dma_stop(struct l1_softc *sc)
{
	dma_context_t		*ctx	= &dma_context[sc->sc_unit];

	itjc_bus_setup(sc);

	/* Only stop the DMA if it is running. */

	if (ctx->state != ITJC_DS_RUNNING)
		return;

	itjc_write_1(TIGER_DMA_OPER, TIGER_DISABLE_DMA);
	DELAY(SEC_DELAY/1000);

	ctx->state = ITJC_DS_STOPPED;
}


/*---------------------------------------------------------------------------*
 *	itjc_bchannel_dma_setup - The DMA side of itjc_bchannel_setup.
 *---------------------------------------------------------------------------*/
static void
itjc_bchannel_dma_setup(struct l1_softc *sc, int h_chan, int activate)
{
	dma_rx_context_t	*rxc  = &dma_rx_context[sc->sc_unit][h_chan];
	dma_tx_context_t	*txc  = &dma_tx_context[sc->sc_unit][h_chan];

	l1_bchan_state_t	*chan = &sc->sc_chan[h_chan];

	u_int8_t		fill_byte,
				*ring_end,
				*cp;

	int			s = SPLI4B();

	itjc_bus_setup(sc);

	if (activate)
	{
		/*
		 * Get the DMA engine going if it's not running already.
		 */
		itjc_dma_start(sc);

		rxc->hdlc_len	= rxc->hdlc_tmp    = rxc->hdlc_crc  = 0;
		rxc->hdlc_ib	= rxc->hdlc_blevel = rxc->hdlc_flag = 0;

		txc->hdlc_tmp	= txc->hdlc_blevel = txc->hdlc_crc  = 0;
		txc->hdlc_ib	= 0;
		txc->hdlc_flag	= 2;
		txc->filled	= 0;

		if (chan->bprot == BPROT_NONE)
			fill_byte = ITJC_TEL_SILENCE_BYTE;
		else
			fill_byte = ITJC_HDLC_ABORT_BYTE;

		ring_end = rxc->ring + ITJC_RING_BYTES;
		for (cp = rxc->ring; cp < ring_end; cp += 4)
			*cp = fill_byte;

		ring_end = txc->ring + ITJC_RING_BYTES;
		for (cp = txc->ring; cp < ring_end; cp += 4)
			*cp = fill_byte;

		rxc->next_read  =
			itjc_get_dma_offset(rxc, TIGER_DMA_RD_CURR_ADDR);

		txc->next_frame = txc->next_write =
			itjc_get_dma_offset(txc, TIGER_DMA_WR_CURR_ADDR);

		rxc->state	= ITJC_RS_ACTIVE;
		txc->state	= ITJC_TS_AFTER_XDU;
	}
	else
	{
		dma_rx_context_t	*rxc2;

		txc->state	= ITJC_TS_IDLE;
		rxc->state	= ITJC_RS_IDLE;

		rxc2 = &dma_rx_context[sc->sc_unit][0];

		if (rxc2->state == ITJC_RS_IDLE 
		&& rxc2[1].state == ITJC_RS_IDLE)
			itjc_dma_stop(sc);
	}

	splx(s);
}


/*---------------------------------------------------------------------------*
 *	Mbuf & if_queues management routines.
 *---------------------------------------------------------------------------*/

static u_int8_t *
itjc_get_rx_mbuf(l1_bchan_state_t *chan, u_int8_t **dst_end_p,
which_mb_t which)
{
	struct mbuf	*mbuf = chan->in_mbuf;

	if (mbuf == NULL && which == ITJC_MB_NEW)
	{
		if ((mbuf = i4b_Bgetmbuf(BCH_MAX_DATALEN)) == NULL)
			panic("itjc_get_rx_mbuf: cannot allocate mbuf!");

		chan->in_mbuf  = mbuf;
		chan->in_cbptr = (u_int8_t *)mbuf->m_data;
		chan->in_len   = 0;
	}

	if (dst_end_p != NULL)
	{
		if (mbuf != NULL)
			*dst_end_p = (u_int8_t *)(mbuf->m_data)
				+ BCH_MAX_DATALEN;
		else
			*dst_end_p = NULL;
	}

	return chan->in_cbptr;
}


static void
itjc_save_rx_mbuf(l1_bchan_state_t *chan, u_int8_t * dst)
{
	struct mbuf	*mbuf = chan->in_mbuf;

	if (dst != NULL && mbuf != NULL)
	{
		chan->in_cbptr = dst;
		chan->in_len   = dst - (u_int8_t *)mbuf->m_data;
	}
	else if (dst == NULL && mbuf == NULL)
	{
		chan->in_cbptr = NULL;
		chan->in_len   = 0;
	}
	else
		panic("itjc_save_rx_mbuf: stale pointer dst=%p mbuf=%p "
			"in_cbptr=%p in_len=%d", dst, mbuf, 
			chan->in_cbptr, chan->in_len);
}


static void
itjc_free_rx_mbuf(l1_bchan_state_t *chan)
{
	struct mbuf	*mbuf = chan->in_mbuf;

	if (mbuf != NULL)
		i4b_Bfreembuf(mbuf);

	chan->in_mbuf  = NULL;
	chan->in_cbptr = NULL;
	chan->in_len   = 0;
}


static void
itjc_put_rx_mbuf(struct l1_softc *sc, l1_bchan_state_t *chan, u_int16_t len)
{
	i4b_trace_hdr_t	hdr;
	struct mbuf	*mbuf	 = chan->in_mbuf;
	u_int8_t	*data	 = mbuf->m_data;
	int		activity = 1;

	mbuf->m_pkthdr.len = mbuf->m_len = len;

	if (sc->sc_trace & TRACE_B_RX)
	{
		hdr.unit = L0ITJCUNIT(sc->sc_unit);
		hdr.type = (chan->channel == HSCX_CH_A ? TRC_CH_B1 : TRC_CH_B2);
		hdr.dir = FROM_NT;
		hdr.count = ++sc->sc_trace_bcount;
		MICROTIME(hdr.time);
		i4b_l1_trace_ind(&hdr, len, data);
	}

	if (chan->bprot == BPROT_NONE)
	{
		activity = ! i4b_l1_bchan_tel_silence(data, len);
				
		/* move rx'd data to rx queue */

		if (! _IF_QFULL(&chan->rx_queue))
		{
			IF_ENQUEUE(&chan->rx_queue, mbuf);
		}
		else
		{
			i4b_Bfreembuf(mbuf);
			len = 0;
		}
	}

	if (len != 0)
	{
		chan->rxcount += len;

		(*chan->isic_drvr_linktab->bch_rx_data_ready)
			(chan->isic_drvr_linktab->unit);
	}
				
	if (activity)
		(*chan->isic_drvr_linktab->bch_activity)
			(chan->isic_drvr_linktab->unit, ACT_RX);

	chan->in_mbuf  = NULL;
	chan->in_cbptr = NULL;
	chan->in_len   = 0;
}


#define itjc_free_tx_mbufs(chan)					\
{									\
	i4b_Bfreembuf((chan)->out_mbuf_head);				\
	(chan)->out_mbuf_cur = (chan)->out_mbuf_head = NULL;		\
	(chan)->out_mbuf_cur_ptr = NULL;				\
	(chan)->out_mbuf_cur_len = 0;					\
}


static u_int16_t
itjc_get_tx_mbuf(struct l1_softc *sc, l1_bchan_state_t *chan,
	u_int8_t **src_p, which_mb_t which)
{
	i4b_trace_hdr_t	hdr;
	struct mbuf	*mbuf = chan->out_mbuf_cur;
	u_int8_t	activity = 1;
	u_int16_t	len;
	void		*data;

	switch (which)
	{
	case ITJC_MB_CURR:
		if (mbuf != NULL)
		{
			*src_p = chan->out_mbuf_cur_ptr;
			return   chan->out_mbuf_cur_len;
		}

		break;

	case ITJC_MB_NEXT:
		if (mbuf != NULL)
		{
			chan->txcount += mbuf->m_len;

			mbuf = mbuf->m_next;

			if (mbuf != NULL)
				goto new_mbuf;
		}

		chan->out_mbuf_cur_ptr = *src_p = NULL;
		chan->out_mbuf_cur_len = 0;

		if (chan->out_mbuf_head != NULL)
		{
			i4b_Bfreembuf(chan->out_mbuf_head);
			chan->out_mbuf_head = NULL;
		}

		return 0;

	case ITJC_MB_NEW:
		if (mbuf != NULL)
			chan->txcount += mbuf->m_len;
	}

	if (chan->out_mbuf_head != NULL)
		i4b_Bfreembuf(chan->out_mbuf_head);

	IF_DEQUEUE(&chan->tx_queue, mbuf);

	if (mbuf == NULL)
	{
		chan->out_mbuf_cur = chan->out_mbuf_head = NULL;
		chan->out_mbuf_cur_ptr = *src_p = NULL;
		chan->out_mbuf_cur_len = 0;

		chan->state &= ~(HSCX_TX_ACTIVE);

		(*chan->isic_drvr_linktab->bch_tx_queue_empty)
			(chan->isic_drvr_linktab->unit);

		return 0;
	}

	chan->out_mbuf_head = mbuf;

new_mbuf:
	chan->out_mbuf_cur	= mbuf;
	chan->out_mbuf_cur_ptr	= data = mbuf->m_data;
	chan->out_mbuf_cur_len	= len  = mbuf->m_len;

	chan->state |= HSCX_TX_ACTIVE;

	if (sc->sc_trace & TRACE_B_TX)
	{
		hdr.unit = L0ITJCUNIT(sc->sc_unit);
		hdr.type = (chan->channel == HSCX_CH_A ? TRC_CH_B1 : TRC_CH_B2);
		hdr.dir = FROM_TE;
		hdr.count = ++sc->sc_trace_bcount;
		MICROTIME(hdr.time);
		i4b_l1_trace_ind(&hdr, len, data);
	}

	if (chan->bprot == BPROT_NONE)
		activity = ! i4b_l1_bchan_tel_silence(data, len);

	if (activity)
		(*chan->isic_drvr_linktab->bch_activity)
			(chan->isic_drvr_linktab->unit, ACT_TX);

	*src_p = data;
	return len;
}


#define itjc_save_tx_mbuf(chan, src, dst)				\
(									\
	(chan)->out_mbuf_cur != NULL ?					\
	(								\
		(chan)->out_mbuf_cur_ptr = (src),			\
		(chan)->out_mbuf_cur_len = (len)			\
	)								\
	:								\
		0							\
)


/*---------------------------------------------------------------------------*
 *	B-channel interrupt service routines.
 *---------------------------------------------------------------------------*/

/*
 * Since the Tiger ASIC doesn't produce a XMIT underflow indication,
 * we need to deduce it ourselves. This is somewhat tricky because we
 * are dealing with modulo m arithmetic. The idea here is to have a
 * "XDU zone" ahead of the writing pointer sized 1/3 of total ring
 * length (a ring slot). If the hardware DMA pointer is found there we
 * consider that a XDU has occurred. To complete the scheme, we never
 * let the ring have more than 2 slots of (unsent) data and adjust the
 * interrupt registers to cause an interrupt at every slot.
 */
static u_int8_t
itjc_xdu(struct l1_softc *sc, l1_bchan_state_t *chan, dma_tx_context_t *ctx,
u_int16_t *dst_p, u_int16_t *dst_end_p, u_int8_t tx_restart)
{
	u_int8_t	xdu;

	u_int16_t	dst_end,
			dst,
			dma,
			dma_l,
			dma_h,
			xdu_l,
			xdu_h;

	itjc_bus_setup(sc);

	/*
	 * Since the hardware is running, be conservative and assume
	 * the pointer location has a `fuzy' error factor.
	 */
	dma   = itjc_get_dma_offset(ctx, TIGER_DMA_WR_CURR_ADDR);
	dma_l = dma;
	dma_h = itjc_ring_add(dma, 1);

	dst_end = itjc_ring_sub(dma_l, ITJC_RING_SLOT_WORDS);

	if (ctx->state != ITJC_TS_ACTIVE)
	{
		xdu = (ctx->state == ITJC_TS_AFTER_XDU);
		dst = itjc_ring_add(dma_h, 4);
		goto done;
	}

	/*
	 * Check for xmit underruns.
	 */
	xdu_l = dst = ctx->next_write; 
	xdu_h = itjc_ring_add(dst, ITJC_RING_SLOT_WORDS);

	if (xdu_l < xdu_h)
		xdu =	   (xdu_l <= dma_l && dma_l < xdu_h)
			|| (xdu_l <= dma_h && dma_h < xdu_h);
	else
		xdu =	   (xdu_l <= dma_l || dma_l < xdu_h)
			|| (xdu_l <= dma_h || dma_h < xdu_h);

	if (xdu)
	{
		ctx->state = ITJC_TS_AFTER_XDU;

		dst = itjc_ring_add(dma_h, 4);
	}
	else if (tx_restart)
	{
		/*
		 * See if we still can restart from immediately
		 * after the last frame sent. It's a XDU test but
		 * using the real data end on the comparsions. We
		 * don't consider XDU an error here because we were
		 * just trying to avoid send a filling gap between
		 * frames. If it's already sent no harm is done.
		 */
		xdu_l = dst = ctx->next_frame; 
		xdu_h = itjc_ring_add(dst, ITJC_RING_SLOT_WORDS);

		if (xdu_l < xdu_h)
			xdu =	   (xdu_l <= dma_l && dma_l < xdu_h)
				|| (xdu_l <= dma_h && dma_h < xdu_h);
		else
			xdu =	   (xdu_l <= dma_l || dma_l < xdu_h)
				|| (xdu_l <= dma_h || dma_h < xdu_h);

		if (xdu)
			dst = itjc_ring_add(dma_h, 4);

		xdu = 0;
	}

done:
	if (dst_p != NULL)
		*dst_p = dst;
	
	if (dst_end_p != NULL)
		*dst_end_p = dst_end;

	ctx->next_write = dst_end;

	return xdu;
}


#define itjc_rotate_hdlc_flag(blevel)					\
	((u_int8_t)(0x7E7E >> (8 - (u_int8_t)((blevel) >> 8))))


static void
itjc_dma_rx_intr(struct l1_softc *sc, l1_bchan_state_t *chan,
dma_rx_context_t *ctx)
{
	u_int8_t	*ring,
			*dst,
			*dst_end,
			flag,
			blevel;

	u_int16_t	dma,
			src,
			tmp2,
			tmp,
			len,
			crc,
			ib;
	
	itjc_bus_setup(sc);


	if (ctx->state == ITJC_RS_IDLE)
		return;

	ring = ctx->ring;
	dma = itjc_get_dma_offset(ctx, TIGER_DMA_RD_CURR_ADDR);
	dma = itjc_ring_sub(dma, 1);
	src = ctx->next_read;

	if (chan->bprot == BPROT_NONE)
	{
		dst = itjc_get_rx_mbuf(chan, &dst_end, ITJC_MB_CURR);

		while (src != dma)
		{
			if (dst == NULL)
				dst = itjc_get_rx_mbuf(chan, &dst_end, 
					ITJC_MB_NEW);

			*dst++ = ring[src];
			src = itjc_ring_add(src, 1);

			if (dst >= dst_end)
			{
				itjc_put_rx_mbuf(sc, chan, BCH_MAX_DATALEN);
				dst = dst_end = NULL;
			}
		}
		ctx->next_read = src;
		itjc_save_rx_mbuf(chan, dst);
		return;
	}

	blevel = ctx->hdlc_blevel;
	flag   = ctx->hdlc_flag;
	len    = ctx->hdlc_len;
	tmp    = ctx->hdlc_tmp;
	crc    = ctx->hdlc_crc;
	ib     = ctx->hdlc_ib;

	dst = itjc_get_rx_mbuf(chan, NULL, ITJC_MB_CURR);

	while (src != dma)
	{
		HDLC_DECODE(*dst++, len, tmp, tmp2, blevel, ib, crc, flag,
		{/* rdd */
			tmp2 = ring[src];
			src = itjc_ring_add(src, 1);
		},
		{/* nfr */
			if (dst != NULL)
				panic("itjc_dma_rx_intr: nfrcmd with "
					"valid current frame");

			dst = itjc_get_rx_mbuf(chan, &dst_end, ITJC_MB_NEW);
			len = dst_end - dst;
		},
		{/* cfr */
			len = BCH_MAX_DATALEN - len;

			if ((!len) || (len > BCH_MAX_DATALEN))
			{
				/*
				 * NOTE: frames without any data, only crc
				 * field, should be silently discared.
				 */
				NDBGL1(L1_S_MSG, "itjc_dma_rx_intr: "
					"bad frame (len=%d, unit=%d)",
					len, sc->sc_unit);

				itjc_free_rx_mbuf(chan);

				goto s0;
			}

			if (crc)
			{
				NDBGL1(L1_S_ERR,
					"CRC (crc=0x%04x, len=%d, unit=%d)",
					crc, len, sc->sc_unit);

				itjc_free_rx_mbuf(chan);

				goto s0;
			}

			itjc_put_rx_mbuf(sc, chan, len);

		s0:
			dst = NULL;
			len = 0;
		},
		{/* rab */
			NDBGL1(L1_S_ERR, "Read Abort (unit=%d)", sc->sc_unit);

			itjc_free_rx_mbuf(chan);
			dst = NULL;
			len = 0;
		},
		{/* rdo */
			NDBGL1(L1_S_ERR, "RDO (unit=%d) dma=%d src=%d",
				sc->sc_unit, dma, src);

			itjc_free_rx_mbuf(chan);
			dst = NULL;
			len = 0;
		},
		continue,
		d);
	}

	itjc_save_rx_mbuf(chan, dst);

	ctx->next_read	= src;
	ctx->hdlc_blevel= blevel;
	ctx->hdlc_flag	= flag;
	ctx->hdlc_len	= len;
	ctx->hdlc_tmp	= tmp;
	ctx->hdlc_crc	= crc;
	ctx->hdlc_ib	= ib;
}


/*
 * The HDLC side of itjc_dma_tx_intr. We made a separate function
 * to improve readability and (perhaps) help the compiler with
 * register allocation.
 */
static void
itjc_hdlc_encode(struct l1_softc *sc, l1_bchan_state_t *chan,
dma_tx_context_t * ctx)
{
	u_int8_t	*ring,
			*src,
			xdu,
			flag,
			flag_byte,
			tx_restart;

	u_int16_t	saved_len,
			dst_end,
			dst_end1,
			dst,
			filled,
			blevel,
			tmp2,
			len,
			crc,
			ib;

	u_int32_t	tmp;


	saved_len = len = itjc_get_tx_mbuf(sc, chan, &src, ITJC_MB_CURR);

	filled = ctx->filled;
	flag   = ctx->hdlc_flag;

	if (src == NULL && flag == 2 && filled >= ITJC_RING_WORDS)
		return;

	tx_restart = (flag == 2 && src != NULL);
	xdu = itjc_xdu(sc, chan, ctx, &dst, &dst_end, tx_restart);

	ring   = ctx->ring;

	ib     = ctx->hdlc_ib;
	crc    = ctx->hdlc_crc;
	tmp    = ctx->hdlc_tmp;
	blevel = ctx->hdlc_blevel;

	if (xdu)
	{
		if (flag != 2)
		{
			NDBGL1(L1_H_XFRERR, "XDU");
			++chan->stat_XDU;

			/*
			 * Abort the current frame and 
			 * prepare for a full restart.
			 */
			itjc_free_tx_mbufs(chan);
			saved_len = len = filled = 0;
			flag = (u_int8_t)-2;
		}
		else if (filled < ITJC_RING_SLOT_WORDS)
		{
			/*
			 * A little garbage may have been retransmitted.
			 * Send an abort before any new data.
			 */
			filled = 0;
			flag = (u_int8_t)-2;
		}
	}

	if (flag != 3)
		len = 0;

	while (dst != dst_end)
	{
		HDLC_ENCODE(
		*src++, len, tmp, tmp2, blevel, ib, crc, flag,
		{/* gfr */
			if ((len = saved_len) == 0)
				len = itjc_get_tx_mbuf(sc, chan, &src,
					ITJC_MB_NEW);

			if (len == 0)
			{
				ctx->next_frame = dst;

				flag_byte = itjc_rotate_hdlc_flag(blevel);

				for (dst_end1 = itjc_ring_sub(dst_end, 1);
				dst != dst_end1;
				dst = itjc_ring_add(dst, 1))
				{
					ring[dst] = flag_byte;
					++filled;
				}
			}
			else
				filled = 0;

			ctx->state = ITJC_TS_ACTIVE;
		},
		{/* nmb */
			saved_len = 0;
			len = itjc_get_tx_mbuf(sc, chan, &src, ITJC_MB_NEXT);
		},
		{/* wrd */
			ring[dst] = (u_int8_t)tmp;
			dst = itjc_ring_add(dst, 1);
		},
		d1);
	}

	ctx->hdlc_blevel = blevel;
	ctx->hdlc_flag   = flag;
	ctx->hdlc_tmp    = tmp;
	ctx->hdlc_crc    = crc;
	ctx->hdlc_ib     = ib;

	ctx->filled = filled;
	ctx->next_write = dst;

	itjc_save_tx_mbuf(chan, src, len);
}


static void
itjc_dma_tx_intr(struct l1_softc *sc, l1_bchan_state_t *chan,
dma_tx_context_t * ctx)
{
	u_int8_t	*data_end,
			*ring,
			*src,
			xdu;

	u_int16_t	dst,
			dst_end,
			filled,
			len;


	if (ctx->state == ITJC_TS_IDLE)
		goto done;

	if (chan->bprot != BPROT_NONE)
	{
		itjc_hdlc_encode(sc, chan, ctx);
		goto done;
	}

	ring   = ctx->ring;
	filled = ctx->filled;

	len = itjc_get_tx_mbuf(sc, chan, &src, ITJC_MB_CURR);

	if (len == 0 && filled >= ITJC_RING_WORDS)
		goto done;

	xdu = itjc_xdu(sc, chan, ctx, &dst, &dst_end, len != 0);

	if (xdu && filled < ITJC_RING_WORDS)
	{
		NDBGL1(L1_H_XFRERR, "XDU");
		++chan->stat_XDU;
		filled = 0;
	}

	if (len == 0)
		goto fill_ring;

	ctx->state = ITJC_TS_ACTIVE;

	data_end = src + len;
	while (dst != dst_end)
	{
		ring[dst] = *src++; --len;

		dst = itjc_ring_add(dst, 1);

		if (src >= data_end)
		{
			len = itjc_get_tx_mbuf(sc, chan, &src, ITJC_MB_NEXT);
			if (len == 0)
				len = itjc_get_tx_mbuf(sc, chan,
					 &src, ITJC_MB_NEW);

			if (len == 0)
			{
				data_end = NULL;
				break;
			}
			data_end = src + len;
		}
	}

	itjc_save_tx_mbuf(chan, src, len);

	filled = 0;

fill_ring:
	ctx->next_frame = dst;

	for (; dst != dst_end; dst = itjc_ring_add(dst, 1))
	{
		ring[dst] = ITJC_TEL_SILENCE_BYTE;
		++filled;
	}

	ctx->next_write = dst;
	ctx->filled = filled;

done:
	return;
}


/*---------------------------------------------------------------------------*
 *	NetJet fifo read/write routines.
 *---------------------------------------------------------------------------*/

static void
itjc_read_fifo(struct l1_softc *sc, int what, void *buf, size_t size)
{
	itjc_bus_setup(sc);

	if (what != ISIC_WHAT_ISAC)
		panic("itjc_write_fifo: Trying to read from HSCX fifo.\n");

	itjc_set_pib_addr_msb(0);
	itjc_read_multi_1(PIB_OFFSET, buf, size);
}


static void
itjc_write_fifo(struct l1_softc *sc, int what, void *buf, size_t size)
{
	itjc_bus_setup(sc);

	if (what != ISIC_WHAT_ISAC)
		panic("itjc_write_fifo: Trying to write to HSCX fifo.\n");

	itjc_set_pib_addr_msb(0);
	itjc_write_multi_1(PIB_OFFSET, buf, size);
}


/*---------------------------------------------------------------------------*
 *	Read an ISAC register.
 *---------------------------------------------------------------------------*/
static u_int8_t
itjc_read_reg(struct l1_softc *sc, int what, bus_size_t offs)
{
	itjc_bus_setup(sc);

	if (what != ISIC_WHAT_ISAC)
	{
		panic("itjc_read_reg: what(%d) != ISIC_WHAT_ISAC\n",
			what);
		return 0;
	}

	itjc_set_pib_addr_msb(offs);
	return itjc_read_1(itjc_pib_2_pci(offs));
}


/*---------------------------------------------------------------------------*
 *	Write an ISAC register.
 *---------------------------------------------------------------------------*/
static void
itjc_write_reg(struct l1_softc *sc, int what, bus_size_t offs, u_int8_t data)
{
	itjc_bus_setup(sc);

	if (what != ISIC_WHAT_ISAC)
	{
		panic("itjc_write_reg: what(%d) != ISIC_WHAT_ISAC\n",
			what);
		return;
	}

	itjc_set_pib_addr_msb(offs);
	itjc_write_1(itjc_pib_2_pci(offs), data);
}


/*---------------------------------------------------------------------------*
 *	itjc_probe - probe for a card.
 *---------------------------------------------------------------------------*/
static int itjc_probe(device_t dev)
{
	u_int16_t	vid = pci_get_vendor(dev),
			did = pci_get_device(dev);

	if ((vid == PCI_TJNET_VID) && (did == PCI_TJ300_DID))
	{
		device_set_desc(dev, "NetJet-S");
		return 0;
	}

	return ENXIO;
}


/*---------------------------------------------------------------------------*
 *	itjc_attach - attach a (previously probed) card.
 *---------------------------------------------------------------------------*/
static int
itjc_attach(device_t dev)
{
	bus_space_handle_t	h;
	bus_space_tag_t		t; 

	struct l1_softc		*sc = device_get_softc(dev);

	u_int16_t		vid = pci_get_vendor(dev),
				did = pci_get_device(dev);

	int			unit = device_get_unit(dev),
				s = splimp(),
				res_init_level = 0,
				error = 0;

	void			*ih = 0;

	dma_context_t		*ctx = &dma_context[unit];
	l1_bchan_state_t	*chan;

	bzero(sc, sizeof(struct l1_softc));

	/* Probably not really required. */
	if (unit >= ITJC_MAXUNIT)
	{
		printf("itjc%d: Error, unit >= ITJC_MAXUNIT!\n", unit);
		splx(s);
		return ENXIO;
	}

	if (!(vid == PCI_TJNET_VID && did == PCI_TJ300_DID))
	{
		printf("itjc%d: unknown device (%04X,%04X)!\n", unit, vid, did);
		goto fail;
	}

	itjc_scp[unit] = sc;

	sc->sc_resources.io_rid[0] = PCIR_BAR(0);
	sc->sc_resources.io_base[0] = bus_alloc_resource_any(dev, 
		SYS_RES_IOPORT, &sc->sc_resources.io_rid[0], RF_ACTIVE);

	if (sc->sc_resources.io_base[0] == NULL)
	{
		printf("itjc%d: couldn't map IO port\n", unit);
		error = ENXIO;
		goto fail;
	}

	h = rman_get_bushandle(sc->sc_resources.io_base[0]);
	t = rman_get_bustag(sc->sc_resources.io_base[0]); 

	++res_init_level;

	/* Allocate interrupt. */
	sc->sc_resources.irq_rid = 0;
	sc->sc_resources.irq = bus_alloc_resource_any(dev, SYS_RES_IRQ,
		&sc->sc_resources.irq_rid, RF_SHAREABLE | RF_ACTIVE);

	if (sc->sc_resources.irq == NULL)
	{
		printf("itjc%d: couldn't map interrupt\n", unit);
		error = ENXIO;
		goto fail;
	}

	++res_init_level;

	error = bus_setup_intr(dev, sc->sc_resources.irq, INTR_TYPE_NET,
			 NULL, itjc_intr, sc, &ih);

	if (error)
	{
		printf("itjc%d: couldn't set up irq handler\n", unit);
		error = ENXIO;
		goto fail;
	}

	/*
	 * Reset the ASIC & the ISAC.
	 */
	itjc_write_1(TIGER_RESET_PIB_CL_TIME, TIGER_RESET_ALL);

	DELAY(SEC_DELAY/100); /* Give it 10 ms to reset ...*/

	itjc_write_1(TIGER_RESET_PIB_CL_TIME,
		TIGER_SELF_ADDR_DMA | TIGER_PIB_3_CYCLES);

	DELAY(SEC_DELAY/100); /* ... and more 10 to recover. */

	/*
	 * First part of DMA initialization. Create & map the memory
	 * pool that will be used to bear the rx & tx ring buffers.
	 */
	ctx->state = ITJC_DS_LOADING;

	error = bus_dma_tag_create(
		NULL,					/* parent */
		4,					/* alignment*/
		0,					/* boundary*/
		BUS_SPACE_MAXADDR_32BIT,		/* lowaddr*/	
		BUS_SPACE_MAXADDR,			/* highaddr*/
		NULL,					/* filter*/
		NULL,					/* filterarg*/
		ITJC_DMA_POOL_BYTES,			/* maxsize*/
		1,					/* nsegments*/
		ITJC_DMA_POOL_BYTES,			/* maxsegsz*/
		BUS_DMA_ALLOCNOW | BUS_DMA_COHERENT,	/* flags*/
		NULL, NULL,				/* lockfuunc, lockarg */
		&ctx->tag);

	if (error)
	{
		printf("itjc%d: couldn't create bus DMA tag.\n", unit);
		goto fail;
	}

	++res_init_level;

        error = bus_dmamem_alloc(
		ctx->tag, 				/* DMA tag */
		(void **)&ctx->pool,	/* KV addr of the allocated memory */
		BUS_DMA_NOWAIT | BUS_DMA_COHERENT,	/* flags */
		&ctx->map);				/* KV <-> PCI map */

	if (error)
                goto fail;

        /*
	 * Load the KV <-> PCI map so the device sees the same
	 * memory segment as pointed by pool. Note: since the
	 * load may happen assyncronously (completion indicated by
	 * the execution of the callback function) we have to
	 * delay the initialization of the DMA engine to a moment we
	 * actually have the proper bus addresses to feed the Tiger
	 * and our DMA control blocks. This will be done in
	 * itjc_bchannel_setup via a call to itjc_dma_start.
	 */
        bus_dmamap_load(
		ctx->tag,		/* DMA tag */
		ctx->map,		/* DMA map */
		ctx->pool,		/* KV addr of buffer */
		ITJC_DMA_POOL_BYTES,	/* buffer size */
		itjc_map_callback, 	/* this receive the bus addr/error */
		ctx, 			/* callback aux arg */
		0);			/* flags */

	++res_init_level;

	/*
	 * Setup the AUX port so we can talk to the ISAC.
	 */
	itjc_write_1(TIGER_AUX_PORT_CNTL, TIGER_AUX_NJ_DEFAULT);
	itjc_write_1(TIGER_INT1_MASK, TIGER_ISAC_INT);

	/*
	 * From now on, almost like a `normal' ISIC driver.
	 */

	sc->sc_unit = unit;

	ISAC_BASE = (caddr_t)ISIC_WHAT_ISAC;

	HSCX_A_BASE = (caddr_t)ISIC_WHAT_HSCXA;
	HSCX_B_BASE = (caddr_t)ISIC_WHAT_HSCXB;

	/* setup access routines */

	sc->clearirq = NULL;
	sc->readreg = itjc_read_reg;
	sc->writereg = itjc_write_reg;

	sc->readfifo = itjc_read_fifo;
	sc->writefifo = itjc_write_fifo;

	/* setup card type */
	
	sc->sc_cardtyp = CARD_TYPEP_NETJET_S;

	/* setup IOM bus type */
	
	sc->sc_bustyp = BUS_TYPE_IOM2;

	/* set up some other miscellaneous things */
	sc->sc_ipac = 0;
	sc->sc_bfifolen = 2 * ITJC_RING_SLOT_WORDS;

	printf("itjc%d: ISAC 2186 Version 1.1 (IOM-2)\n", unit);

	/* init the ISAC */
	itjc_isac_init(sc);

	chan = &sc->sc_chan[HSCX_CH_A];
	if(!mtx_initialized(&chan->rx_queue.ifq_mtx))
		mtx_init(&chan->rx_queue.ifq_mtx, "i4b_avma1pp_rx", NULL, MTX_DEF);
	if(!mtx_initialized(&chan->tx_queue.ifq_mtx))
		mtx_init(&chan->tx_queue.ifq_mtx, "i4b_avma1pp_tx", NULL, MTX_DEF);
	chan = &sc->sc_chan[HSCX_CH_B];
	if(!mtx_initialized(&chan->rx_queue.ifq_mtx))
		mtx_init(&chan->rx_queue.ifq_mtx, "i4b_avma1pp_rx", NULL, MTX_DEF);
	if(!mtx_initialized(&chan->tx_queue.ifq_mtx))
		mtx_init(&chan->tx_queue.ifq_mtx, "i4b_avma1pp_tx", NULL, MTX_DEF);

	/* init the "HSCX" */
	itjc_bchannel_setup(sc->sc_unit, HSCX_CH_A, BPROT_NONE, 0);
	
	itjc_bchannel_setup(sc->sc_unit, HSCX_CH_B, BPROT_NONE, 0);

	/* can't use the normal B-Channel stuff */
	itjc_init_linktab(sc);

	/* set trace level */

	sc->sc_trace = TRACE_OFF;

	sc->sc_state = ISAC_IDLE;

	sc->sc_ibuf = NULL;
	sc->sc_ib = NULL;
	sc->sc_ilen = 0;

	sc->sc_obuf = NULL;
	sc->sc_op = NULL;
	sc->sc_ol = 0;
	sc->sc_freeflag = 0;

	sc->sc_obuf2 = NULL;
	sc->sc_freeflag2 = 0;

#if defined(__FreeBSD__) && __FreeBSD__ >=3
	callout_handle_init(&sc->sc_T3_callout);
	callout_handle_init(&sc->sc_T4_callout);	
#endif
	
	/* init higher protocol layers */
	
	i4b_l1_mph_status_ind(L0ITJCUNIT(sc->sc_unit), STI_ATTACH, 
		sc->sc_cardtyp, &itjc_l1mux_func);

	splx(s);
	return 0;

  fail:
	switch (res_init_level)
	{
	case 5:
		bus_dmamap_unload(ctx->tag, ctx->map);
		/* FALL TRHU */

	case 4:
		bus_dmamem_free(ctx->tag, ctx->pool, ctx->map);
		bus_dmamap_destroy(ctx->tag, ctx->map);
		/* FALL TRHU */

	case 3:
		bus_dma_tag_destroy(ctx->tag);
		/* FALL TRHU */

	case 2:
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sc_resources.irq);
		/* FALL TRHU */

	case 1:
		bus_release_resource(dev, SYS_RES_IOPORT, PCIR_BAR(0),
			sc->sc_resources.io_base[0]);
		/* FALL TRHU */

	case 0:
		break;
	}

	itjc_scp[unit] = NULL;

	splx(s);
	return error;
}


/*---------------------------------------------------------------------------*
 *	itjc_intr - main interrupt service routine.
 *---------------------------------------------------------------------------*/
static void
itjc_intr(void *xsc)
{
	struct l1_softc		*sc	= xsc;
	l1_bchan_state_t	*chan	= &sc->sc_chan[0];
	dma_context_t		*dma	= &dma_context[sc->sc_unit];
	dma_rx_context_t	*rxc	= &dma_rx_context[sc->sc_unit][0];
	dma_tx_context_t	*txc	= &dma_tx_context[sc->sc_unit][0];

	itjc_bus_setup(sc);

	/* Honor interrupts from successfully configured cards only. */
	if (dma->state < ITJC_DS_STOPPED)
		return;

	/* First, we check the ISAC... */
	if (! (itjc_read_1(TIGER_AUX_PORT_DATA) & TIGER_ISAC_INT_MASK))
	{
		itjc_write_1(TIGER_INT1_STATUS, TIGER_ISAC_INT);
		NDBGL1(L1_H_IRQ, "ISAC");
		itjc_isac_intr(sc);
	}

	/* ... after what we always have a look at the DMA rings. */

	NDBGL1(L1_H_IRQ, "Tiger");

	itjc_read_1(TIGER_INT0_STATUS);
	itjc_write_1(TIGER_INT0_STATUS, TIGER_TARGET_ABORT_INT
		| TIGER_MASTER_ABORT_INT | TIGER_RD_END_INT
		| TIGER_RD_INT_INT	 | TIGER_WR_END_INT | TIGER_WR_INT_INT);

	itjc_dma_rx_intr(sc, chan, rxc);
	itjc_dma_tx_intr(sc, chan, txc);

	++chan; ++rxc; ++txc;

	itjc_dma_rx_intr(sc, chan, rxc);
	itjc_dma_tx_intr(sc, chan, txc);
}


/*---------------------------------------------------------------------------*
 *	itjc_bchannel_setup - (Re)initialize and start/stop a Bchannel.
 *---------------------------------------------------------------------------*/
static void
itjc_bchannel_setup(int unit, int h_chan, int bprot, int activate)
{
#ifdef __FreeBSD__
	struct l1_softc		*sc	= itjc_scp[unit];
#else
	struct l1_softc		*sc	= isic_find_sc(unit);
#endif
	l1_bchan_state_t	*chan	= &sc->sc_chan[h_chan];
	int			s	= SPLI4B();
		
	NDBGL1(L1_BCHAN, "unit=%d, channel=%d, %s",
		unit, h_chan, activate ? "activate" : "deactivate");

	/*
	 * If we are deactivating the channel, we have to stop
	 * the DMA before we reset the channel control structures.
	 */
	if (! activate)
		itjc_bchannel_dma_setup(sc, h_chan, activate); 

	/* general part */

	chan->state = HSCX_IDLE;

	chan->unit = sc->sc_unit;	/* unit number */
	chan->channel = h_chan;		/* B channel */
	chan->bprot = bprot;		/* B channel protocol */

	/* receiver part */

	i4b_Bcleanifq(&chan->rx_queue);	/* clean rx queue */

	chan->rx_queue.ifq_maxlen = IFQ_MAXLEN;

	chan->rxcount = 0;		/* reset rx counter */
	
	i4b_Bfreembuf(chan->in_mbuf);	/* clean rx mbuf */

	chan->in_mbuf = NULL;		/* reset mbuf ptr */
	chan->in_cbptr = NULL;		/* reset mbuf curr ptr */
	chan->in_len = 0;		/* reset mbuf data len */
	
	/* transmitter part */

	i4b_Bcleanifq(&chan->tx_queue);	/* clean tx queue */

	chan->tx_queue.ifq_maxlen = IFQ_MAXLEN;
	
	chan->txcount = 0;		/* reset tx counter */
	
	i4b_Bfreembuf(chan->out_mbuf_head);	/* clean tx mbuf */

	chan->out_mbuf_head = NULL;	/* reset head mbuf ptr */
	chan->out_mbuf_cur = NULL;	/* reset current mbuf ptr */	
	chan->out_mbuf_cur_ptr = NULL;	/* reset current mbuf data ptr */
	chan->out_mbuf_cur_len = 0;	/* reset current mbuf data cnt */

	/*
	 * Only setup & start the DMA after all other channel
	 * control structures are in place.
	 */
	if (activate)
		itjc_bchannel_dma_setup(sc, h_chan, activate); 

	splx(s);
}


/*---------------------------------------------------------------------------*
 *	itjc_bchannel_start - Signal us we have more data to send.
 *---------------------------------------------------------------------------*/
static void
itjc_bchannel_start(int unit, int h_chan)
{
#if 0 /* Buggy code */
	/*
	 * I disabled this routine because it was causing crashes when
	 * this driver was used with the ISP (kernel SPPP) protocol driver.
	 * The scenario is reproductible:
	 *	Use the -link1 (dial on demand) ifconfig option.
	 *	Start an interactive  TCP connection to somewhere.
	 *	Wait until the PPP connection times out and is dropped.
	 *	Try to send something on the TCP connection.
	 *	The machine will print some garbage and halt or reboot
	 *	(no panic messages).
	 *
	 * I've nailed down the problem to the fact that this routine
	 * was being called before the B channel had been setup again.
	 *
	 * For now, I don't have a good solution other than this one.
	 * But, don't despair. The impact of it is unnoticeable.
	 */

#ifdef __FreeBSD__
	struct l1_softc  *sc	= itjc_scp[unit];
#else
	struct l1_softc	 *sc	= isic_find_sc(unit);
#endif
	l1_bchan_state_t *chan	= &sc->sc_chan[h_chan];

	int		 s	= SPLI4B();

	dma_tx_context_t *txc	= &dma_tx_context[unit][h_chan];

	if (chan->state & HSCX_TX_ACTIVE)
	{
		splx(s);
		return;
	}

	itjc_dma_tx_intr(sc, chan, txc);

	splx(s);
#endif
}


/*---------------------------------------------------------------------------*
 *	itjc_shutdown - Stop the driver and reset the card.
 *---------------------------------------------------------------------------*/
static void
itjc_shutdown(device_t dev)
{
	struct l1_softc *sc = device_get_softc(dev);

	itjc_bus_setup(sc);

	/*
	 * Stop the DMA the nice and easy way.
	 */
	itjc_bchannel_setup(sc->sc_unit, 0, BPROT_NONE, 0);
	itjc_bchannel_setup(sc->sc_unit, 1, BPROT_NONE, 0);

	/*
	 * Reset the card.
	 */
	itjc_write_1(TIGER_RESET_PIB_CL_TIME, TIGER_RESET_ALL);

	DELAY(SEC_DELAY/100); /* Give it 10 ms to reset ...*/

	itjc_write_1(TIGER_RESET_PIB_CL_TIME,
		TIGER_SELF_ADDR_DMA | TIGER_LATCH_DMA_INT | TIGER_PIB_3_CYCLES);

	DELAY(SEC_DELAY/100); /* ... and more 10 to recover */
}


/*---------------------------------------------------------------------------*
 *	itjc_ret_linktab - Return the address of itjc drivers linktab.
 *---------------------------------------------------------------------------*/
isdn_link_t *
itjc_ret_linktab(int unit, int channel)
{
#ifdef __FreeBSD__
	struct l1_softc		*sc = itjc_scp[unit];
#else
	struct l1_softc		*sc = isic_find_sc(unit);
#endif
	l1_bchan_state_t	*chan = &sc->sc_chan[channel];

	return(&chan->isic_isdn_linktab);
}
 
/*---------------------------------------------------------------------------*
 *	itjc_set_linktab - Set the driver linktab in the b channel softc.
 *---------------------------------------------------------------------------*/
void
itjc_set_linktab(int unit, int channel, drvr_link_t *dlt)
{
#ifdef __FreeBSD__
	struct l1_softc *sc	= itjc_scp[unit];
#else
	struct l1_softc *sc	= isic_find_sc(unit);
#endif
	l1_bchan_state_t *chan	= &sc->sc_chan[channel];

	chan->isic_drvr_linktab = dlt;
}


/*---------------------------------------------------------------------------*
 *	itjc_init_linktab - Initialize our local linktab.
 *---------------------------------------------------------------------------*/
static void
itjc_init_linktab(struct l1_softc *sc)
{
	l1_bchan_state_t *chan = &sc->sc_chan[HSCX_CH_A];
	isdn_link_t *lt = &chan->isic_isdn_linktab;

	/* make sure the hardware driver is known to layer 4 */
	/* avoid overwriting if already set */
	if (ctrl_types[CTRL_PASSIVE].set_linktab == NULL)
	{
		ctrl_types[CTRL_PASSIVE].set_linktab = itjc_set_linktab;
		ctrl_types[CTRL_PASSIVE].get_linktab = itjc_ret_linktab;
	}

	/* local setup */
	lt->unit = sc->sc_unit;
	lt->channel = HSCX_CH_A;
	lt->bch_config = itjc_bchannel_setup;
	lt->bch_tx_start = itjc_bchannel_start;
	lt->bch_stat = itjc_bchannel_stat;
	lt->tx_queue = &chan->tx_queue;

	/* used by non-HDLC data transfers, i.e. telephony drivers */
	lt->rx_queue = &chan->rx_queue;

	/* used by HDLC data transfers, i.e. ipr and isp drivers */	
	lt->rx_mbuf = &chan->in_mbuf;	
                                                
	chan = &sc->sc_chan[HSCX_CH_B];
	lt = &chan->isic_isdn_linktab;

	lt->unit = sc->sc_unit;
	lt->channel = HSCX_CH_B;
	lt->bch_config = itjc_bchannel_setup;
	lt->bch_tx_start = itjc_bchannel_start;
	lt->bch_stat = itjc_bchannel_stat;
	lt->tx_queue = &chan->tx_queue;

	/* used by non-HDLC data transfers, i.e. telephony drivers */
	lt->rx_queue = &chan->rx_queue;

	/* used by HDLC data transfers, i.e. ipr and isp drivers */	
	lt->rx_mbuf = &chan->in_mbuf;	
}


/*---------------------------------------------------------------------------*
 *	itjc_bchannel_stat - Collect link statistics for a given B channel.
 *---------------------------------------------------------------------------*/
static void
itjc_bchannel_stat(int unit, int h_chan, bchan_statistics_t *bsp)
{
#ifdef __FreeBSD__
	struct l1_softc *sc = itjc_scp[unit];
#else
	struct l1_softc *sc = isic_find_sc(unit);
#endif
	l1_bchan_state_t *chan = &sc->sc_chan[h_chan];
	int s;

	s = SPLI4B();
	
	bsp->outbytes = chan->txcount;
	bsp->inbytes = chan->rxcount;

	chan->txcount = 0;
	chan->rxcount = 0;

	splx(s);
}


/*---------------------------------------------------------------------------*
 *	Netjet - ISAC interrupt routine.
 *---------------------------------------------------------------------------*/
static void
itjc_isac_intr(struct l1_softc *sc)
{
	register u_char irq_stat;

	do
	{
		/* get isac irq status */
		irq_stat = ISAC_READ(I_ISTA);

		if(irq_stat)
			itjc_isac_irq(sc, irq_stat); /* isac handler */
	}
	while(irq_stat);

	ISAC_WRITE(I_MASK, 0xff);

	DELAY(100);

	ISAC_WRITE(I_MASK, ISAC_IMASK);
}


/*---------------------------------------------------------------------------*
 *	itjc_recover - Try to recover from ISAC irq lockup.
 *---------------------------------------------------------------------------*/
void
itjc_recover(struct l1_softc *sc)
{
	u_char byte;
	
	/* get isac irq status */

	byte = ISAC_READ(I_ISTA);

	NDBGL1(L1_ERROR, "  ISAC: ISTA = 0x%x", byte);
	
	if(byte & ISAC_ISTA_EXI)
		NDBGL1(L1_ERROR, "  ISAC: EXIR = 0x%x", (u_char)ISAC_READ(I_EXIR));

	if(byte & ISAC_ISTA_CISQ)
	{
		byte = ISAC_READ(I_CIRR);
	
		NDBGL1(L1_ERROR, "  ISAC: CISQ = 0x%x", byte);
		
		if(byte & ISAC_CIRR_SQC)
			NDBGL1(L1_ERROR, "  ISAC: SQRR = 0x%x", (u_char)ISAC_READ(I_SQRR));
	}

	NDBGL1(L1_ERROR, "  ISAC: IMASK = 0x%x", ISAC_IMASK);

	ISAC_WRITE(I_MASK, 0xff);	
	DELAY(100);
	ISAC_WRITE(I_MASK, ISAC_IMASK);
}
