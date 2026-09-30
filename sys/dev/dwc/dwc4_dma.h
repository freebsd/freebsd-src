/*-
 * Copyright (c) 2026 Ruslan Bukin <br@bsdpad.com>
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

#ifndef _DEV_DWC_DWC4_DMA_H_
#define _DEV_DWC_DWC4_DMA_H_

#define	ETH_DMAMR				0x1000
#define	 DMAMR_SWR	(1 << 0) /* Software Reset */
#define	ETH_DMASBMR				0x1004
#define	 DMASBMR_RD_OSR_LMT_S	16 /* AXI Maximum Read Outstanding Req Limit */
#define	 DMASBMR_AAL	(1 << 12) /* Address-Aligned Beats */
#define	 DMASBMR_EAME	(1 << 11) /* Enhanced Address Mode Enable */
#define	 DMASBMR_BLEN16	(1 << 3) /* Fixed Burst Length */
#define	 DMASBMR_BLEN8	(1 << 2) /* Fixed Burst Length */
#define	 DMASBMR_BLEN4	(1 << 1) /* Fixed Burst Length */
#define	 DMASBMR_FB	(1 << 0) /* Fixed Burst Length */
#define	ETH_DMAISR				0x1008
#define	ETH_DMADS1R				0x100C
#define	ETH_DMADS2R				0x1010
#define	ETH_DMAA4TXACR				0x1020
#define	ETH_DMAA4RXACR				0x1024
#define	ETH_DMAA4DACR				0x1028
#define	ETH_DMALPIEI				0x1040
#define	ETH_DMATBSCTRLR(x)			(0x1050 + 0x04 * (x))

/* DMA Channel */
#define	ETH_DMACCR(x)				(0x1100 + 0x80 * (x))
#define	 DMACCR_PBLX8	(1 << 16) /* 8xPBL mode */
#define	 DMACCR_DSL_S	18
#define	ETH_DMACTXCR(x)				(0x1104 + 0x80 * (x))
#define	 DMACTXCR_EDSE	(1 << 28) /* Enhanced Descriptor Enable */
#define	 DMACTXCR_TXPBL_S	16 /* Transmit Programmable Burst Length */
#define	 DMACTXCR_TXPBL_M	(0x3f << DMACTXCR_TXPBL_S)
#define	 DMACTXCR_OSF	(1 << 4) /* Operate on Second Packet */
#define	 DMACTXCR_ST	(1 << 0) /* Start or Stop Transmission Command */
#define	ETH_DMACRXCR(x)				(0x1108 + 0x80 * (x))
#define	 DMACRXCR_RXPBL_S	16 /* Receive Programmable Burst Length */
#define	 DMACRXCR_RXPBL_M	(0x3f << DMACRXCR_RXPBL_S)
#define	 DMACRXCR_RBSZ_S	1 /* Receive Buffer size */
#define	 DMACRXCR_RBSZ_M	(0x3fff << DMACRXCR_RBSZ_S)
#define	 DMACRXCR_SR	(1 << 0) /* Start or Stop Receive */
#define	ETH_DMACTXDLAR_HI(x)			(0x1110 + 0x80 * (x))
#define	ETH_DMACTXDLAR(x)			(0x1114 + 0x80 * (x))
#define	ETH_DMACRXDLAR_HI(x)			(0x1118 + 0x80 * (x))
#define	ETH_DMACRXDLAR(x)			(0x111C + 0x80 * (x))
#define	ETH_DMACTXDTPR(x)			(0x1120 + 0x80 * (x))
#define	ETH_DMACRXDTPR(x)			(0x1128 + 0x80 * (x))
#define	ETH_DMACTXRLR(x)			(0x112C + 0x80 * (x))
#define	ETH_DMACRXRLR(x)			(0x1130 + 0x80 * (x))
#define	ETH_DMACIER(x)				(0x1134 + 0x80 * (x))
#define	 DMACIER_TIE	(1 << 0) /* Transmit Interrupt Enable */
#define	 DMACIER_TXSE	(1 << 1) /* Transmit Stopped Enable */
#define	 DMACIER_TBUE	(1 << 2) /* Transmit Buffer Unavailable Enable */
#define	 DMACIER_RIE	(1 << 6) /* Receive Interrupt Enable */
#define	 DMACIER_RBUE	(1 << 7) /* Receive Buffer Unavailable Enable */
#define	 DMACIER_RSE	(1 << 8) /* Receive Stopped Enable */
#define	 DMACIER_RWTE	(1 << 9) /* Receive Watchdog Timeout Enable */
#define	 DMACIER_ETIE	(1 << 10) /* Early Transmit Interrupt Enable */
#define	 DMACIER_ERIE	(1 << 11) /* Early Receive Interrupt Enable */
#define	 DMACIER_FBEE	(1 << 12) /* Fatal Bus Error Enable */
#define	 DMACIER_CDEE	(1 << 13) /* Context Descriptor Error Enable */
#define	 DMACIER_AIE	(1 << 14) /* Abnormal Interrupt Summary Enable */
#define	 DMACIER_NIE	(1 << 15) /* Normal Interrupt Summary Enable */
#define	ETH_DMACRXIWTR(x)			(0x1138 + 0x80 * (x))
#define	ETH_DMACSFCSR(x)			(0x113C + 0x80 * (x))
#define	ETH_DMACCATXDR(x)			(0x1144 + 0x80 * (x))
#define	ETH_DMACCARXDR(x)			(0x114C + 0x80 * (x))
#define	ETH_DMACCATXBR(x)			(0x1154 + 0x80 * (x))
#define	ETH_DMACCARXBR(x)			(0x115C + 0x80 * (x))
#define	ETH_DMACSR(x)				(0x1160 + 0x80 * (x))
#define	 DMACSR_NIS	(1 << 15) /* Normal Interrupt Summary */
#define	 DMACSR_AIS	(1 << 14) /* AIS: Abnormal Interrupt Summary */
#define	 DMACSR_FBE	(1 << 12) /* Fatal Bus Error */
#define	 DMACSR_RI	(1 << 6) /* Receive Interrupt */
#define	 DMACSR_TI	(1 << 0) /* Transmit Interrupt */
#define	ETH_DMACMFCR(x)				(0x1164 + 0x80 * (x))

int dma4_init(struct dwc_softc *sc);
void dma4_free(struct dwc_softc *sc);
void dma4_start(struct dwc_softc *sc);
void dma4_stop(struct dwc_softc *sc);
int dma4_reset(struct dwc_softc *sc);
int dma4_setup_txbuf(struct dwc_softc *sc, int idx, struct mbuf **mp);
void dma4_txfinish_locked(struct dwc_softc *sc);
void dma4_rxfinish_locked(struct dwc_softc *sc);
void dma4_txstart(struct dwc_softc *sc);
int dma4_intr(struct dwc_softc *sc);

#endif	/* !_DEV_DWC_DWC4_DMA_H_ */
