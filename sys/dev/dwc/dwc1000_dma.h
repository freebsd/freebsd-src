/*-
 * Copyright (c) 2014 Ruslan Bukin <br@bsdpad.com>
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef	__DWC1000_DMA_H__
#define	 __DWC1000_DMA_H__

/*
 * A hardware buffer descriptor.  Rx and Tx buffers have the same descriptor
 * layout, but the bits in the fields have different meanings.
 */
struct dwc_hwdesc
{
	uint32_t desc0;
	uint32_t desc1;
	uint32_t addr1;		/* ptr to first buffer data */
	uint32_t addr2;		/* ptr to next descriptor / second buffer data*/
};

int dma1000_init(struct dwc_softc *sc);
void dma1000_free(struct dwc_softc *sc);
void dma1000_start(struct dwc_softc *sc);
void dma1000_stop(struct dwc_softc *sc);
int dma1000_reset(struct dwc_softc *sc);
int dma1000_setup_txbuf(struct dwc_softc *sc, int idx, struct mbuf **mp);
void dma1000_txfinish_locked(struct dwc_softc *sc);
void dma1000_rxfinish_locked(struct dwc_softc *sc);
void dma1000_txstart(struct dwc_softc *sc);
int dma1000_intr(struct dwc_softc *sc);

#endif	/* __DWC1000_DMA_H__ */
