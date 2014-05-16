/* $FreeBSD$ */
/*-
 * Copyright (c) 2014 Hans Petter Selasky <hselasky@FreeBSD.org>
 * All rights reserved.
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

#ifndef _SAF1761_DCI_REG_H_
#define	_SAF1761_DCI_REG_H_

/* Global registers */

#define	SOTG_VEND_ID 0x370
#define	SOTG_PROD_ID 0x372
#define	SOTG_CTRL_SET 0x374
#define	SOTG_CTRL_CLR 0x376
#define	SOTG_CTRL_OTG_DISABLE (1 << 10)
#define	SOTG_CTRL_OTG_SE0_EN (1 << 9)
#define	SOTG_CTRL_BDIS_ACON_EN (1 << 8)
#define	SOTG_CTRL_SW_SEL_HC_DC (1 << 7)
#define	SOTG_CTRL_VBUS_CHRG (1 << 6)
#define	SOTG_CTRL_VBUS_DISCHRG (1 << 5)
#define	SOTG_CTRL_VBUS_DRV (1 << 4)
#define	SOTG_CTRL_SEL_CP_EXT (1 << 3)
#define	SOTG_CTRL_DM_PULL_DOWN (1 << 2)
#define	SOTG_CTRL_DP_PULL_DOWN (1 << 1)
#define	SOTG_CTRL_DP_PULL_UP (1 << 0)
#define	SOTG_STATUS 0x378
#define	SOTG_STATUS_B_SE0_SRP (1 << 8)
#define	SOTG_STATUS_B_SESS_END (1 << 7)
#define	SOTG_STATUS_RMT_CONN (1 << 4)
#define	SOTG_STATUS_ID (1 << 3)
#define	SOTG_STATUS_DP_SRP (1 << 2)
#define	SOTG_STATUS_A_B_SESS_VLD (1 << 1)
#define	SOTG_STATUS_VBUS_VLD (1 << 0)
#define	SOTG_IRQ_LATCH_SET 0x37C
#define	SOTG_IRQ_LATCH_CLR 0x37E
#define	SOTG_IRQ_ENABLE_SET 0x380
#define	SOTG_IRQ_ENABLE_CLR 0x382
#define	SOTG_IRQ_RISE_SET 0x384
#define	SOTG_IRQ_RISE_CLR 0x386
#define	SOTG_IRQ_OTG_TMR_TIMEOUT (1 << 9)
#define	SOTG_IRQ_B_SE0_SRP (1 << 8)
#define	SOTG_IRQ_B_SESS_END (1 << 7)
#define	SOTG_IRQ_BDIS_ACON (1 << 6)
#define	SOTG_IRQ_OTG_RESUME (1 << 5)
#define	SOTG_IRQ_RMT_CONN (1 << 4)
#define	SOTG_IRQ_ID (1 << 3)
#define	SOTG_IRQ_DP_SRP (1 << 2)
#define	SOTG_IRQ_A_B_SESS_VLD (1 << 1)
#define	SOTG_IRQ_VBUS_VLD (1 << 0)
#define	SOTG_TIMER_LOW_SET 0x388
#define	SOTG_TIMER_LOW_CLR 0x38A
#define	SOTG_TIMER_HIGH_SET 0x38C
#define	SOTG_TIMER_HIGH_CLR 0x38E
#define	SOTG_TIMER_START_TMR (1U << 15)

/* Peripheral controller specific registers */

#define	SOTG_ADDRESS 0x200
#define	SOTG_ADDRESS_ENABLE (1 << 7)
#define	SOTG_MODE 0x20C
#define	SOTG_MODE_DMACLK_ON (1 << 9)
#define	SOTG_MODE_VBUSSTAT (1 << 8)
#define	SOTG_MODE_CLKAON (1 << 7)
#define	SOTG_MODE_SNDRSU (1 << 6)
#define	SOTG_MODE_GOSUSP (1 << 5)
#define	SOTG_MODE_SFRESET (1 << 4)
#define	SOTG_MODE_GLINTENA (1 << 3)
#define	SOTG_MODE_WKUPCS (1 << 2)
#define	SOTG_INTERRUPT_CFG  0x210
#define	SOTG_INTERRUPT_CFG_CDBGMOD (3 << 6)
#define	SOTG_INTERRUPT_CFG_DDBGMODIN (3 << 4)
#define	SOTG_INTERRUPT_CFG_DDBGMODOUT (3 << 2)
#define	SOTG_INTERRUPT_CFG_INTLVL (1 << 1)
#define	SOTG_INTERRUPT_CFG_INTPOL (1 << 0)
#define	SOTG_DEBUG 0x212
#define	SOTG_DEBUG_SET (1 << 0)
#define	SOTG_DCINTERRUPT_EN 0x214
#define	SOTG_HW_MODE_CTRL 0x300
#define	SOTG_HW_MODE_CTRL_ALL_ATX_RESET (1 << 31)
#define	SOTG_HW_MODE_CTRL_ANA_DIGI_OC (1 << 15)
#define	SOTG_HW_MODE_CTRL_DEV_DMA (1 << 11)
#define	SOTG_HW_MODE_CTRL_COMN_INT (1 << 10)
#define	SOTG_HW_MODE_CTRL_COMN_DMA (1 << 9)
#define	SOTG_HW_MODE_CTRL_DATA_BUS_WIDTH (1 << 8)
#define	SOTG_HW_MODE_CTRL_DACK_POL (1 << 6)
#define	SOTG_HW_MODE_CTRL_DREQ_POL (1 << 5)
#define	SOTG_HW_MODE_CTRL_INTR_POL (1 << 2)
#define	SOTG_HW_MODE_CTRL_INTR_LEVEL (1 << 1)
#define	SOTG_HW_MODE_CTRL_GLOBAL_INTR_EN (1 << 0)
#define	SOTG_OTG_CTRL 0x374
#define	SOTG_EP_INDEX 0x22c
#define	SOTG_EP_INDEX_EP0SETUP (1 << 5)
#define	SOTG_EP_INDEX_ENDP_INDEX_MASK (15 << 1)
#define	SOTG_EP_INDEX_ENDP_INDEX_SHIFT 1
#define	SOTG_EP_INDEX_DIR_IN (1 << 0)
#define	SOTG_EP_INDEX_DIR_OUT 0
#define	SOTG_CTRL_FUNC 0x228
#define	SOTG_CTRL_FUNC_CLBUF (1 << 4)
#define	SOTG_CTRL_FUNC_VENDP (1 << 3)
#define	SOTG_CTRL_FUNC_DSEN (1 << 2)
#define	SOTG_CTRL_FUNC_STATUS (1 << 1)
#define	SOTG_CTRL_FUNC_STALL (1 << 0)
#define	SOTG_DATA_PORT 0x220
#define	SOTG_BUF_LENGTH 0x21C
#define	SOTG_DCBUFFERSTATUS 0x21E
#define	SOTG_DCBUFFERSTATUS_FILLED_MASK (3 << 0)
#define	SOTG_EP_MAXPACKET 0x204
#define	SOTG_EP_TYPE 0x208
#define	SOTG_EP_TYPE_NOEMPPKT (1 << 4)
#define	SOTG_EP_TYPE_ENABLE (1 << 3)
#define	SOTG_EP_TYPE_DBLBUF (1 << 2)
#define	SOTG_EP_TYPE_EP_TYPE (3 << 0)
#define	SOTG_DMA_CMD 0x230
#define	SOTG_DMA_XFER_COUNT 0x234
#define	SOTG_DCDMA_CFG 0x238
#define	SOTG_DMA_HW 0x23C
#define	SOTG_DMA_IRQ_REASON 0x250
#define	SOTG_DMA_IRQ_ENABLE 0x254
#define	SOTG_DMA_EP 0x258
#define	SOTG_BURST_COUNTER 0x264
#define	SOTG_DCINTERRUPT 0x218
#define	SOTG_DCINTERRUPT_IEPRX(n) (1 << (10 + (2*(n))))
#define	SOTG_DCINTERRUPT_IEPTX(n) (1 << (11 + (2*(n))))
#define	SOTG_DCINTERRUPT_IEP0SETUP (1 << 8)
#define	SOTG_DCINTERRUPT_IEVBUS (1 << 7)
#define	SOTG_DCINTERRUPT_IEDMA (1 << 6)
#define	SOTG_DCINTERRUPT_IEHS_STA (1 << 5)
#define	SOTG_DCINTERRUPT_IERESM (1 << 4)
#define	SOTG_DCINTERRUPT_IESUSP (1 << 3)
#define	SOTG_DCINTERRUPT_IEPSOF (1 << 2)
#define	SOTG_DCINTERRUPT_IESOF (1 << 1)
#define	SOTG_DCINTERRUPT_IEBRST (1 << 0)
#define	SOTG_DCCHIP_ID 0x270
#define	SOTG_FRAME_NUM 0x274
#define	SOTG_FRAME_NUM_MICROSOFR_MASK 0x3800
#define	SOTG_FRAME_NUM_MICROSOFR_SHIFT 11
#define	SOTG_FRAME_NUM_SOFR_MASK 0x7FF
#define	SOTG_DCSCRATCH 0x278
#define	SOTG_UNLOCK_DEVICE 0x27C
#define	SOTG_UNLOCK_DEVICE_CODE 0xAA37
#define	SOTG_IRQ_PULSE_WIDTH 0x280
#define	SOTG_TEST_MODE 0x284
#define	SOTG_TEST_MODE_FORCEHS (1 << 7)
#define	SOTG_TEST_MODE_FORCEFS (1 << 4)
#define	SOTG_TEST_MODE_PRBS (1 << 3)
#define	SOTG_TEST_MODE_KSTATE (1 << 2)
#define	SOTG_TEST_MODE_JSTATE (1 << 1)
#define	SOTG_TEST_MODE_SE0_NAK (1 << 0)

/* Host controller specific registers */

#define	SOTG_CONFIGFLAG 0x0060
#define	SOTG_CONFIGFLAG_ENABLE (1 << 0)
#define	SOTG_PORTSC1 0x0064
#define	SOTG_PORTSC1_PO (1 << 13)
#define	SOTG_PORTSC1_PP (1 << 12)
#define	SOTG_PORTSC1_PR (1 << 8)
#define	SOTG_PORTSC1_SUSP (1 << 7)
#define	SOTG_PORTSC1_FPR (1 << 6)
#define	SOTG_PORTSC1_PED (1 << 2)
#define	SOTG_PORTSC1_ECSC (1 << 1)
#define	SOTG_PORTSC1_ECCS (1 << 0)
#define	SOTG_ASYNC_PDT(x) (0x400 + (60 * 1024) + ((x) * 32))
#define	SOTG_INTR_PDT(x) (0x400 + (61 * 1024) + ((x) * 32))
#define	SOTG_ISOC_PDT(x) (0x400 + (62 * 1024) + ((x) * 32))
#define	SOTG_HC_MEMORY_ADDR(x) (((x) - 0x400) >> 3)

#endif					/* _SAF1761_DCI_REG_H_ */
