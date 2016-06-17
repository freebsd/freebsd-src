/*****************************************************************************/

/*
 *	smdma.h  --  soundcard radio modem driver dma buffer routines.
 *
 *	Copyright (C) 1996  Thomas Sailer (sailer@ife.ee.ethz.ch)
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Please note that the GPL allows you to use the driver, NOT the radio.
 *  In order to use the radio, you need a license from the communications
 *  authority of your country.
 *
 */

#ifndef _SMDMA_H
#define _SMDMA_H

/* ---------------------------------------------------------------------- */

#include "sm.h"

/* ---------------------------------------------------------------------- */

#define DMA_MODE_AUTOINIT      0x10
#define NUM_FRAGMENTS          4

/*
 * NOTE: make sure that hdlcdrv_hdlcbuffer contains enough space
 * for the modulator to fill the whole DMA buffer without underrun
 * at the highest possible baud rate, otherwise the TX state machine will
 * not work correctly. That is (9k6 FSK): HDLCDRV_HDLCBUFFER > 6*NUM_FRAGMENTS
 */ 

/* --------------------------------------------------------------------- */
/*
 * ===================== DMA buffer management ===========================
 */

/*
 * returns the number of samples per fragment
 */
static inline unsigned int dma_setup(struct sm_state *sm, int send, unsigned int dmanr)
{
	if (send) {
		disable_dma(dmanr);
		clear_dma_ff(dmanr);
		set_dma_mode(dmanr, DMA_MODE_WRITE | DMA_MODE_AUTOINIT);
		set_dma_addr(dmanr, virt_to_bus(sm->dma.obuf));
		set_dma_count(dmanr, sm->dma.ofragsz * NUM_FRAGMENTS);
		enable_dma(dmanr);
		if (sm->dma.o16bit)
			return sm->dma.ofragsz/2;
		return sm->dma.ofragsz;
	} else {
		disable_dma(dmanr);
		clear_dma_ff(dmanr);
		set_dma_mode(dmanr, DMA_MODE_READ | DMA_MODE_AUTOINIT);
		set_dma_addr(dmanr, virt_to_bus(sm->dma.ibuf));
		set_dma_count(dmanr, sm->dma.ifragsz * NUM_FRAGMENTS);
		enable_dma(dmanr);
		if (sm->dma.i16bit)
			return sm->dma.ifragsz/2;
		return sm->dma.ifragsz;
	}
}

/* --------------------------------------------------------------------- */

static inline unsigned int dma_ptr(struct sm_state *sm, int send, unsigned int dmanr,
				       unsigned int *curfrag)
{
	unsigned int dmaptr, sz, frg, offs;
	
	dmaptr = get_dma_residue(dmanr);
	if (send) {
		sz = sm->dma.ofragsz * NUM_FRAGMENTS;
		if (dmaptr == 0 || dmaptr > sz)
			dmaptr = sz;
		dmaptr--;
		frg = dmaptr / sm->dma.ofragsz;
		offs = (dmaptr % sm->dma.ofragsz) + 1;
		*curfrag = NUM_FRAGMENTS - 1 - frg;
#ifdef SM_DEBUG
		if (!sm->debug_vals.dma_residue || offs < sm->debug_vals.dma_residue)
			sm->debug_vals.dma_residue = offs;
#endif /* SM_DEBUG */
		if (sm->dma.o16bit)
			return offs/2;
		return offs;
	} else {
		sz = sm->dma.ifragsz * NUM_FRAGMENTS;
		if (dmaptr == 0 || dmaptr > sz)
			dmaptr = sz;
		dmaptr--;
		frg = dmaptr / sm->dma.ifragsz;
		offs = (dmaptr % sm->dma.ifragsz) + 1;
		*curfrag = NUM_FRAGMENTS - 1 - frg;
#ifdef SM_DEBUG
		if (!sm->debug_vals.dma_residue || offs < sm->debug_vals.dma_residue)
			sm->debug_vals.dma_residue = offs;
#endif /* SM_DEBUG */
		if (sm->dma.i16bit)
			return offs/2;
		return offs;
	}
}

/* --------------------------------------------------------------------- */

static inline int dma_end_transmit(struct sm_state *sm, unsigned int curfrag)
{
	unsigned int diff = (NUM_FRAGMENTS + curfrag - sm->dma.ofragptr) % NUM_FRAGMENTS;

	sm->dma.ofragptr = curfrag;
	if (sm->dma.ptt_cnt <= 0) {
		sm->dma.ptt_cnt = 0;
		return 0;
	}
	sm->dma.ptt_cnt -= diff;
	if (sm->dma.ptt_cnt <= 0) {
		sm->dma.ptt_cnt = 0;
		return -1;
	}
	return 0;
}

static inline void dma_transmit(struct sm_state *sm)
{
	void *p;

	while (sm->dma.ptt_cnt < NUM_FRAGMENTS && hdlcdrv_ptt(&sm->hdrv)) {
		p = (unsigned char *)sm->dma.obuf + sm->dma.ofragsz *
			((sm->dma.ofragptr + sm->dma.ptt_cnt) % NUM_FRAGMENTS);
		if (sm->dma.o16bit) {
			time_exec(sm->debug_vals.mod_cyc, 
				  sm->mode_tx->modulator_s16(sm, p, sm->dma.ofragsz/2));
		} else {
			time_exec(sm->debug_vals.mod_cyc, 
				  sm->mode_tx->modulator_u8(sm, p, sm->dma.ofragsz));
		}
		sm->dma.ptt_cnt++;
	}
}

static inline void dma_init_transmit(struct sm_state *sm)
{
	sm->dma.ofragptr = 0;
	sm->dma.ptt_cnt = 0;
}

static inline void dma_start_transmit(struct sm_state *sm)
{
	sm->dma.ofragptr = 0;
	if (sm->dma.o16bit) {
		time_exec(sm->debug_vals.mod_cyc, 
			  sm->mode_tx->modulator_s16(sm, sm->dma.obuf, sm->dma.ofragsz/2));
	} else {
		time_exec(sm->debug_vals.mod_cyc, 
			  sm->mode_tx->modulator_u8(sm, sm->dma.obuf, sm->dma.ofragsz));
	}
	sm->dma.ptt_cnt = 1;
}

static inline void dma_clear_transmit(struct sm_state *sm)
{
	sm->dma.ptt_cnt = 0;
	memset(sm->dma.obuf, (sm->dma.o16bit) ? 0 : 0x80, sm->dma.ofragsz * NUM_FRAGMENTS);
}

/* --------------------------------------------------------------------- */

static inline void dma_receive(struct sm_state *sm, unsigned int curfrag)
{
	void *p;

	while (sm->dma.ifragptr != curfrag) {
		if (sm->dma.ifragptr)
			p = (unsigned char *)sm->dma.ibuf + 
				sm->dma.ifragsz * sm->dma.ifragptr;
		else {
			p = (unsigned char *)sm->dma.ibuf + NUM_FRAGMENTS * sm->dma.ifragsz;
			memcpy(p, sm->dma.ibuf, sm->dma.ifragsz);
		}
		if (sm->dma.o16bit) {
			time_exec(sm->debug_vals.demod_cyc, 
				  sm->mode_rx->demodulator_s16(sm, p, sm->dma.ifragsz/2));
		} else {
			time_exec(sm->debug_vals.demod_cyc, 
				  sm->mode_rx->demodulator_u8(sm, p, sm->dma.ifragsz));
		}
		sm->dma.ifragptr = (sm->dma.ifragptr + 1) % NUM_FRAGMENTS;
	}
}

static inline void dma_init_receive(struct sm_state *sm)
{
	sm->dma.ifragptr = 0;
}

/* --------------------------------------------------------------------- */
#endif /* _SMDMA_H */



