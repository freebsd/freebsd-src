/* $FreeBSD$ */
/*-
 * Copyright (c) 2012 Hans Petter Selasky. All rights reserved.
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

#ifndef _DWC_OTG_H_
#define	_DWC_OTG_H_

#define	DWC_OTG_MAX_DEVICES (USB_MIN_DEVICES + 1)
#define	DWC_OTG_FRAME_MASK 0x7FF
#define	DWC_OTG_MAX_TXP 4
#define	DWC_OTG_MAX_TXN (0x200 * DWC_OTG_MAX_TXP)

/* Global CSR registers */

#define	DWC_OTG_REG_GOTGCTL		0x0000
#define	DWC_OTG_MSK_GOTGCTL_CHIRP_ON		(1U << 27)
#define	DWC_OTG_MSK_GOTGCTL_BSESS_VALID		(1U << 19)
#define	DWC_OTG_MSK_GOTGCTL_ASESS_VALID		(1U << 18)
#define	DWC_OTG_MSK_GOTGCTL_CONN_ID_STATUS	(1U << 16)
#define	DWC_OTG_MSK_GOTGCTL_SESS_REQ		(1U << 1)
#define	DWC_OTG_MSK_GOTGCTL_SESS_VALID		(1U << 0)

#define	DWC_OTG_REG_GOTGINT		0x0004
#define	DWC_OTG_REG_GAHBCFG		0x0008
#define	DWC_OTG_MSK_GAHBCFG_GLOBAL_IRQ		(1U << 0)

#define	DWC_OTG_REG_GUSBCFG		0x000C
#define	DWC_OTG_MSK_GUSBCFG_FORCE_DEVICE	(1U << 30)
#define	DWC_OTG_MSK_GUSBCFG_FORCE_HOST		(1U << 29)
#define	DWC_OTG_MSK_GUSBCFG_NO_PULLUP		(1U << 27)
#define	DWC_OTG_MSK_GUSBCFG_NO_PULLUP		(1U << 27)
#define	DWC_OTG_MSK_GUSBCFG_IC_USB_CAP		(1U << 26)
#define	DWC_OTG_MSK_GUSBCFG_ULPI_FS_LS		(1U << 17)
#define	DWC_OTG_MSK_GUSBCFG_TRD_TIM(x)		(((x) & 15U) << 10)
#define	DWC_OTG_MSK_GUSBCFG_HRP			(1U << 9)
#define	DWC_OTG_MSK_GUSBCFG_SRP			(1U << 8)
#define	DWC_OTG_MSK_GUSBCFG_HS_PHY		(1U << 6)
#define	DWC_OTG_MSK_GUSBCFG_FS_INTF		(1U << 5)
#define	DWC_OTG_MSK_GUSBCFG_ULPI_UMTI_SEL	(1U << 4)
#define	DWC_OTG_MSK_GUSBCFG_PHY_INTF		(1U << 3)

#define	DWC_OTG_REG_GRSTCTL		0x0010
#define	DWC_OTG_MSK_GRSTCTL_TXFIFO(n)		(((n) & 31U) << 6)
#define	DWC_OTG_MSK_GRSTCTL_TXFFLUSH		(1U << 5)
#define	DWC_OTG_MSK_GRSTCTL_RXFFLUSH		(1U << 4)
#define	DWC_OTG_MSK_GRSTCTL_FRMCNTRRST		(1U << 2)
#define	DWC_OTG_MSK_GRSTCTL_CSFTRST		(1U << 0)

#define	DWC_OTG_REG_GINTSTS		0x0014
#define	DWC_OTG_REG_GINTMSK		0x0018
#define	DWC_OTG_MSK_GINT_WKUPINT		(1U << 31)
#define	DWC_OTG_MSK_GINT_SESSREQINT		(1U << 30)
#define	DWC_OTG_MSK_GINT_DISCONNINT		(1U << 29)
#define	DWC_OTG_MSK_GINT_CONNIDSTSCHNG		(1U << 28)
#define	DWC_OTG_MSK_GINT_LPM			(1U << 27)
#define	DWC_OTG_MSK_GINT_PTXFEMP		(1U << 26)
#define	DWC_OTG_MSK_GINT_HCHINT			(1U << 25)
#define	DWC_OTG_MSK_GINT_PRTINT			(1U << 24)
#define	DWC_OTG_MSK_GINT_RESETDET		(1U << 23)
#define	DWC_OTG_MSK_GINT_FETSUSP		(1U << 22)
#define	DWC_OTG_MSK_GINT_INCOMPL_P		(1U << 21)
#define	DWC_OTG_MSK_GINT_INCOMPL_ISO_IN		(1U << 20)
#define	DWC_OTG_MSK_GINT_OUTEP			(1U << 19)
#define	DWC_OTG_MSK_GINT_INEP			(1U << 18)
#define	DWC_OTG_MSK_GINT_EP_MISMATCH		(1U << 17)
#define	DWC_OTG_MSK_GINT_RESTORE_DONE		(1U << 16)
#define	DWC_OTG_MSK_GINT_EOP_FRAME		(1U << 15)
#define	DWC_OTG_MSK_GINT_ISO_OUT_DROP		(1U << 14)
#define	DWC_OTG_MSK_GINT_ENUM_DONE		(1U << 13)
#define	DWC_OTG_MSK_GINT_USB_RESET		(1U << 12)
#define	DWC_OTG_MSK_GINT_USB_SUSPEND		(1U << 11)
#define	DWC_OTG_MSK_GINT_EARLY_SUSPEND		(1U << 10)
#define	DWC_OTG_MSK_GINT_I2C_INT		(1U << 9)
#define	DWC_OTG_MSK_GINT_ULPI_CARKIT		(1U << 8)
#define	DWC_OTG_MSK_GINT_GLOBAL_OUT_NAK		(1U << 7)
#define	DWC_OTG_MSK_GINT_GLOBAL_IN_NAK		(1U << 6)
#define	DWC_OTG_MSK_GINT_NPTXFEMP		(1U << 5)
#define	DWC_OTG_MSK_GINT_RXFLVL			(1U << 4)
#define	DWC_OTG_MSK_GINT_SOF			(1U << 3)
#define	DWC_OTG_MSK_GINT_OTG			(1U << 2)
#define	DWC_OTG_MSK_GINT_MODE_MISMATCH		(1U << 1)
#define	DWC_OTG_MSK_GINT_CUR_MODE		(1U << 0)

#define	DWC_OTG_REG_GRXSTSR		0x001C
#define	DWC_OTG_REG_GRXSTSP		0x0020
#define	DWC_OTG_MSK_GRXSTS_PACKET_STS		(15U << 17)
#define	DWC_OTG_MSK_GRXSTS_HST_IN_DATA		(2U << 17)
#define	DWC_OTG_MSK_GRXSTS_HST_IN_COMPLETE	(3U << 17)
#define	DWC_OTG_MSK_GRXSTS_HST_DT_ERROR		(5U << 17)
#define	DWC_OTG_MSK_GRXSTS_HST_HALTED		(7U << 17)
#define	DWC_OTG_MSK_GRXSTS_DEV_GLOB_OUT_NAK	(1U << 17)
#define	DWC_OTG_MSK_GRXSTS_DEV_OUT_DATA		(2U << 17)
#define	DWC_OTG_MSK_GRXSTS_DEV_OUT_COMPLETE	(3U << 17)
#define	DWC_OTG_MSK_GRXSTS_DEV_STP_COMPLETE	(4U << 17)
#define	DWC_OTG_MSK_GRXSTS_DEV_STP_DATA		(6U << 17)
#define	DWC_OTG_MSK_GRXSTS_PID			(3U << 15)
#define	DWC_OTG_MSK_GRXSTS_PID_DATA0		(0U << 15)
#define	DWC_OTG_MSK_GRXSTS_PID_DATA1		(2U << 15)
#define	DWC_OTG_MSK_GRXSTS_PID_DATA2		(1U << 15)
#define	DWC_OTG_MSK_GRXSTS_PID_MDATA		(3U << 15)
#define	DWC_OTG_MSK_GRXSTS_GET_BYTE_CNT(x)	(((x) >> 4) & 0x7FFU)
#define	DWC_OTG_MSK_GRXSTS_GET_CHANNEL(x)	((x) & 15U)
#define	DWC_OTG_MSK_GRXSTS_GET_FRNUM	(x)	(((x) >> 21) & 15U)

#define	DWC_OTG_REG_GRXFSIZ		0x0024
#define	DWC_OTG_REG_GNPTXFSIZ		0x0028
#define	DWC_OTG_REG_GNPTXSTS		0x002C
#define	DWC_OTG_REG_GI2CCTL		0x0030
#define	DWC_OTG_REG_GPVNDCTL		0x0034
#define	DWC_OTG_REG_GGPIO		0x0038
#define	DWC_OTG_REG_GUID		0x003C
#define	DWC_OTG_REG_GSNPSID		0x0040
#define	DWC_OTG_REG_GHWCFG1		0x0044
#define	DWC_OTG_MSK_GHWCFG1_GET_DIR(x, n)	(((x) >> (2 * (n))) & 3U)
#define	DWC_OTG_MSK_GHWCFG1_BIDIR		(0U)
#define	DWC_OTG_MSK_GHWCFG1_IN			(1U)
#define	DWC_OTG_MSK_GHWCFG1_OUT			(2U)
#define	DWC_OTG_REG_GHWCFG2		0x0048
#define	DWC_OTG_MSK_GHWCFG2_NUM_DEV_EP(x)	((((x) >> 10) & 15) + 1)
#define	DWC_OTG_MSK_GHWCFG2_NUM_HOST_EP(x)	((((x) >> 14) & 15) + 1)
#define	DWC_OTG_MSK_GHWCFG2_DYN_FIFO		(1U << 19)
#define	DWC_OTG_MSK_GHWCFG2_MPI			(1U << 20)
#define	DWC_OTG_REG_GHWCFG3		0x004C
#define	DWC_OTG_MSK_GHWCFG3_GET_DFIFO(x)	((x) >> 16)
#define	DWC_OTG_MSK_GHWCFG3_PKT_SIZE		(0x10U << (((x) >> 4) & 7))
#define	DWC_OTG_MSK_GHWCFG3_XFR_SIZE		(0x400U << (((x) >> 0) & 15))

#define	DWC_OTG_REG_GHWCFG4		0x0050
#define	DWC_OTG_MSK_GHWCFG4_NUM_IN_EPS(x)	((((x) >> 26) & 15U) + 1U)
#define	DWC_OTG_MSK_GHWCFG4_NUM_CTRL_EPS(x)	(((x) >> 16) & 15U)
#define	DWC_OTG_MSK_GHWCFG4_NUM_IN_PERIODIC_EPS(x)	(((x) >> 0) & 15U)

#define	DWC_OTG_REG_GLPMCFG		0x0054
#define	DWC_OTG_MSK_GLPMCFG_HSIC_CONN		(1U << 30)
#define	DWC_OTG_REG_GPWRDN		0x0058
#define	DWC_OTG_MSK_GPWRDN_BVALID		(1U << 22)
#define	DWC_OTG_MSK_GPWRDN_IDDIG		(1U << 21)
#define	DWC_OTG_MSK_GPWRDN_CONNDET_INT		(1U << 14)
#define	DWC_OTG_MSK_GPWRDN_CONNDET		(1U << 13)
#define	DWC_OTG_MSK_GPWRDN_DISCONN_INT		(1U << 12)
#define	DWC_OTG_MSK_GPWRDN_DISCONN		(1U << 11)
#define	DWC_OTG_MSK_GPWRDN_RESETDET_INT		(1U << 10)
#define	DWC_OTG_MSK_GPWRDN_RESETDET		(1U << 9)
#define	DWC_OTG_MSK_GPWRDN_LINESTATE_INT	(1U << 8)
#define	DWC_OTG_MSK_GPWRDN_LINESTATE		(1U << 7)
#define	DWC_OTG_MSK_GPWRDN_DISABLE_VBUS		(1U << 6)
#define	DWC_OTG_MSK_GPWRDN_POWER_DOWN		(1U << 5)
#define	DWC_OTG_MSK_GPWRDN_POWER_DOWN_RST	(1U << 4)
#define	DWC_OTG_MSK_GPWRDN_POWER_DOWN_CLAMP	(1U << 3)
#define	DWC_OTG_MSK_GPWRDN_RESTORE		(1U << 2)
#define	DWC_OTG_MSK_GPWRDN_PMU_ACTIVE		(1U << 1)
#define	DWC_OTG_MSK_GPWRDN_PMU_IRQ_SEL		(1U << 0)

#define	DWC_OTG_REG_GDFIFOCFG		0x005C
#define	DWC_OTG_REG_GADPCTL		0x0060
#define	DWC_OTG_REG_HPTXFSIZ		0x0100
#define	DWC_OTG_REG_DPTXFSIZ(n)			(0x0100 + (4*(n)))
#define	DWC_OTG_REG_DIEPTXF(n)			(0x0100 + (4*(n)))

/* Host Mode CSR registers */

#define	DWC_OTG_REG_HCFG		0x0400
#define	DWC_OTG_REG_HFIR		0x0404
#define	DWC_OTG_REG_HFNUM		0x0408
#define	DWC_OTG_REG_HPTXSTS		0x0410
#define	DWC_OTG_REG_HAINT		0x0414
#define	DWC_OTG_REG_HAINTMSK		0x0418
#define	DWC_OTG_REG_HPRT		0x0440
#define	DWC_OTG_REG_HCCHAR(n)	(0x0500 + (32*(n)))
#define	DWC_OTG_REG_HCSPLT(n)	(0x0504 + (32*(n)))
#define	DWC_OTG_REG_HCINT(n)	(0x0508 + (32*(n)))
#define	DWC_OTG_REG_HCINTMSK(n)	(0x050C + (32*(n)))
#define	DWC_OTG_REG_HCTSIZ(n)	(0x0510 + (32*(n)))
#define	DWC_OTG_REG_HCDMA(n)	(0x0514 + (32*(n)))
#define	DWC_OTG_REG_HCDMAB(n)	(0x051C + (32*(n)))

/* Device Mode CSR registers */

#define	DWC_OTG_REG_DCFG		0x0800
#define	DWC_OTG_MSK_DCFG_SET_DEV_ADDR(x) (((x) & 0x7FU) << 4)
#define	DWC_OTG_MSK_DCFG_SET_DEV_SPEED(x) ((x) & 0x3U)
#define	DWC_OTG_MSK_DCFG_DEV_SPEED_HI		(0U)
#define	DWC_OTG_MSK_DCFG_DEV_SPEED_FULL20	(1U)
#define	DWC_OTG_MSK_DCFG_DEV_SPEED_FULL10	(3U)

#define	DWC_OTG_REG_DCTL		0x0804
#define	DWC_OTG_MSK_DCTL_PWRON_PROG_DONE	(1U << 11)
#define	DWC_OTG_MSK_DCTL_CGOUT_NAK		(1U << 10)
#define	DWC_OTG_MSK_DCTL_CGNPIN_NAK		(1U << 8)
#define	DWC_OTG_MSK_DCTL_SOFT_DISC		(1U << 1)
#define	DWC_OTG_MSK_DCTL_REMOTE_WAKEUP		(1U << 0)

#define	DWC_OTG_REG_DSTS		0x0808
#define	DWC_OTG_MSK_DSTS_GET_FNUM(x)		(((x) >> 8) & 0x3FFF)
#define	DWC_OTG_MSK_DSTS_GET_ENUM_SPEED(x)	(((x) >> 1) & 3U)
#define	DWC_OTG_MSK_DSTS_ENUM_SPEED_HI		(0U)
#define	DWC_OTG_MSK_DSTS_ENUM_SPEED_FULL20	(1U)
#define	DWC_OTG_MSK_DSTS_ENUM_SPEED_LOW10	(2U)
#define	DWC_OTG_MSK_DSTS_ENUM_SPEED_FULL10	(3U)
#define	DWC_OTG_MSK_DSTS_SUSPEND		(1U << 0)

#define	DWC_OTG_REG_DIEPMSK		0x0810
#define	DWC_OTG_MSK_DIEP_FIFO_EMPTY		(1U << 4)
#define	DWC_OTG_MSK_DIEP_XFER_COMPLETE		(1U << 0)

#define	DWC_OTG_REG_DOEPMSK		0x0814
#define	DWC_OTG_MSK_DOEP_FIFO_EMPTY		(1U << 4)
#define	DWC_OTG_MSK_DOEP_XFER_COMPLETE		(1U << 0)

#define	DWC_OTG_REG_DAINT		0x0818
#define	DWC_OTG_REG_DAINTMSK		0x081C

#define	DWC_OTG_MSK_ENDPOINT(x,in) \
	((in) ? (1U << ((x) & 15U)) : \
	 (0x10000U << ((x) & 15U)))

#define	DWC_OTG_REG_DTKNQR1		0x0820
#define	DWC_OTG_REG_DTKNQR2		0x0824
#define	DWC_OTG_REG_DTKNQR3		0x0830
#define	DWC_OTG_REG_DTKNQR4		0x0834
#define	DWC_OTG_REG_DVBUSDIS		0x0828
#define	DWC_OTG_REG_DVBUSPULSE		0x082C
#define	DWC_OTG_REG_DTHRCTL		0x0830
#define	DWC_OTG_REG_DIEPEMPMSK		0x0834
#define	DWC_OTG_REG_DEACHINT		0x0838
#define	DWC_OTG_REG_DEACHINTMSK		0x083C
#define	DWC_OTG_REG_DIEPEACHMSK(n)	(0x0840 + (4*(n)))
#define	DWC_OTG_REG_DOEPEACHMSK(n)	(0x0880 + (4*(n)))

#define	DWC_OTG_REG_DIEPCTL(n)		(0x0900 + (32*(n)))
#define	DWC_OTG_MSK_DIEPCTL_ENABLE		(1U << 31)
#define	DWC_OTG_MSK_DIEPCTL_DISABLE		(1U << 30)
#define	DWC_OTG_MSK_DIEPCTL_SET_DATA1		(1U << 29)	/* non-control */
#define	DWC_OTG_MSK_DIEPCTL_SET_DATA0		(1U << 28)	/* non-control */
#define	DWC_OTG_MSK_DIEPCTL_SET_NAK		(1U << 27)
#define	DWC_OTG_MSK_DIEPCTL_CLR_NAK		(1U << 26)
#define	DWC_OTG_MSK_DIEPCTL_FNUM(n)		(((n) & 15U) << 22)
#define	DWC_OTG_MSK_DIEPCTL_STALL		(1U << 21)
#define	DWC_OTG_MSK_EP_SET_TYPE(n)	(((n) & 3) << 18)
#define	DWC_OTG_MSK_EP_TYPE_CONTROL		(0U)
#define	DWC_OTG_MSK_EP_TYPE_ISOC		(1U)
#define	DWC_OTG_MSK_EP_TYPE_BULK		(2U)
#define	DWC_OTG_MSK_EP_TYPE_INTERRUPT		(3U)
#define	DWC_OTG_MSK_DIEPCTL_USB_AEP		(1U << 15)
#define	DWC_OTG_MSK_DIEPCTL_MPS_64		(0U << 0)	/* control-only */
#define	DWC_OTG_MSK_DIEPCTL_MPS_32		(1U << 0)	/* control-only */
#define	DWC_OTG_MSK_DIEPCTL_MPS_16		(2U << 0)	/* control-only */
#define	DWC_OTG_MSK_DIEPCTL_MPS_8		(3U << 0)	/* control-only */
#define	DWC_OTG_MSK_DIEPCTL_MPS(n)		((n) & 0x7FF)	/* non-control */

#define	DWC_OTG_REG_DIEPINT(n)		(0x0908 + (32*(n)))
#define	DWC_OTG_MSK_DXEPINT_TXFEMP		(1U << 7)
#define	DWC_OTG_MSK_DXEPINT_SETUP		(1U << 3)
#define	DWC_OTG_MSK_DXEPINT_XFER_COMPL		(1U << 0)

#define	DWC_OTG_REG_DIEPTSIZ(n)		(0x0910 + (32*(n)))
#define	DWC_OTG_MSK_DXEPTSIZ_SET_MULTI(n)	(((n) & 3) << 29)
#define	DWC_OTG_MSK_DXEPTSIZ_SET_NPKT(n)	(((n) & 0x3FF) << 19)
#define	DWC_OTG_MSK_DXEPTSIZ_GET_NPKT(n)	(((n) >> 19) & 0x3FF)
#define	DWC_OTG_MSK_DXEPTSIZ_SET_NBYTES(n)	(((n) & 0x7FFFFF) << 0)
#define	DWC_OTG_MSK_DXEPTSIZ_GET_NBYTES(n)	(((n) >> 0) & 0x7FFFFF)

#define	DWC_OTG_REG_DIEPDMA(n)		(0x0914 + (32*(n)))
#define	DWC_OTG_REG_DTXFSTS(n)		(0x0918 + (32*(n)))
#define	DWC_OTG_REG_DIEPDMAB0		(0x091C + (32*(n)))

#define	DWC_OTG_REG_DOEPCTL(n)		(0x0B00 + (32*(n)))
#define	DWC_OTG_MSK_DOEPCTL_ENABLE		(1U << 31)
#define	DWC_OTG_MSK_DOEPCTL_DISABLE		(1U << 30)
#define	DWC_OTG_MSK_DOEPCTL_SET_DATA1		(1U << 29)	/* non-control */
#define	DWC_OTG_MSK_DOEPCTL_SET_DATA0		(1U << 28)	/* non-control */
#define	DWC_OTG_MSK_DOEPCTL_SET_NAK		(1U << 27)
#define	DWC_OTG_MSK_DOEPCTL_CLR_NAK		(1U << 26)
#define	DWC_OTG_MSK_DOEPCTL_FNUM(n)		(((n) & 15U) << 22)
#define	DWC_OTG_MSK_DOEPCTL_STALL		(1U << 21)
#define	DWC_OTG_MSK_DOEPCTL_EP_TYPE(n)	(((n) & 3) << 18)
#define	DWC_OTG_MSK_DOEPCTL_USB_AEP		(1U << 15)
#define	DWC_OTG_MSK_DOEPCTL_MPS_64		(0U << 0)	/* control-only */
#define	DWC_OTG_MSK_DOEPCTL_MPS_32		(1U << 0)	/* control-only */
#define	DWC_OTG_MSK_DOEPCTL_MPS_16		(2U << 0)	/* control-only */
#define	DWC_OTG_MSK_DOEPCTL_MPS_8		(3U << 0)	/* control-only */
#define	DWC_OTG_MSK_DOEPCTL_MPS(n)	((n) & 0x7FF)	/* non-control */

#define	DWC_OTG_REG_DOEPINT(n)		(0x0B08 + (32*(n)))
#define	DWC_OTG_REG_DOEPTSIZ(n)		(0x0B10 + (32*(n)))
#define	DWC_OTG_REG_DOEPDMA(n)		(0x0B14 + (32*(n)))
#define	DWC_OTG_REG_DOEPDMAB(n)		(0x0B1C + (32*(n)))

/* FIFO access registers */

#define	DWC_OTG_REG_DFIFO(n)		(0x1000 + (0x1000 * (n)))

/* Power and clock gating CSR */

#define	DWC_OTG_REG_PCGCCTL		0x0E00

#define	DWC_OTG_READ_4(sc, reg) \
  bus_space_read_4((sc)->sc_io_tag, (sc)->sc_io_hdl, reg)

#define	DWC_OTG_WRITE_4(sc, reg, data)	\
  bus_space_write_4((sc)->sc_io_tag, (sc)->sc_io_hdl, reg, data)

struct dwc_otg_td;
struct dwc_otg_softc;

typedef uint8_t (dwc_otg_cmd_t)(struct dwc_otg_td *td);

struct dwc_otg_td {
	struct dwc_otg_td *obj_next;
	dwc_otg_cmd_t *func;
	struct usb_page_cache *pc;
	uint32_t tx_bytes;
	uint32_t offset;
	uint32_t remainder;
	uint16_t max_packet_size;	/* packet_size */
	uint16_t npkt;
	uint8_t	ep_no;
	uint8_t	error:1;
	uint8_t	alt_next:1;
	uint8_t	short_pkt:1;
	uint8_t	did_stall:1;
};

struct dwc_otg_std_temp {
	dwc_otg_cmd_t *func;
	struct usb_page_cache *pc;
	struct dwc_otg_td *td;
	struct dwc_otg_td *td_next;
	uint32_t len;
	uint32_t offset;
	uint16_t max_frame_size;
	uint8_t	short_pkt;
	/*
	 * short_pkt = 0: transfer should be short terminated
	 * short_pkt = 1: transfer should not be short terminated
	 */
	uint8_t	setup_alt_next;
	uint8_t did_stall;
	uint8_t bulk_or_control;
};

struct dwc_otg_config_desc {
	struct usb_config_descriptor confd;
	struct usb_interface_descriptor ifcd;
	struct usb_endpoint_descriptor endpd;
} __packed;

union dwc_otg_hub_temp {
	uWord	wValue;
	struct usb_port_status ps;
};

struct dwc_otg_flags {
	uint8_t	change_connect:1;
	uint8_t	change_suspend:1;
	uint8_t	status_suspend:1;	/* set if suspended */
	uint8_t	status_vbus:1;		/* set if present */
	uint8_t	status_bus_reset:1;	/* set if reset complete */
	uint8_t	status_high_speed:1;	/* set if High Speed is selected */
	uint8_t	remote_wakeup:1;
	uint8_t	self_powered:1;
	uint8_t	clocks_off:1;
	uint8_t	port_powered:1;
	uint8_t	port_enabled:1;
	uint8_t	d_pulled_up:1;
};

struct dwc_otg_profile {
	struct usb_hw_ep_profile usb;
	uint16_t max_buffer;
};

struct dwc_otg_softc {
	struct usb_bus sc_bus;
	union dwc_otg_hub_temp sc_hub_temp;
	struct dwc_otg_profile sc_hw_ep_profile[16];

	struct usb_device *sc_devices[DWC_OTG_MAX_DEVICES];
	struct resource *sc_io_res;
	struct resource *sc_irq_res;
	void   *sc_intr_hdl;
	bus_size_t sc_io_size;
	bus_space_tag_t sc_io_tag;
	bus_space_handle_t sc_io_hdl;

	uint32_t sc_rx_bounce_buffer[1024 / 4];
	uint32_t sc_tx_bounce_buffer[(512 * DWC_OTG_MAX_TXP) / 4];

	uint32_t sc_fifo_size;
	uint32_t sc_irq_mask;
	uint32_t sc_last_rx_status;
	uint32_t sc_out_ctl[16];
	uint32_t sc_in_ctl[16];

	uint16_t sc_active_out_ep;

	uint8_t	sc_dev_ep_max;
	uint8_t sc_dev_in_ep_max;
	uint8_t	sc_rt_addr;		/* root HUB address */
	uint8_t	sc_conf;		/* root HUB config */

	uint8_t	sc_hub_idata[1];

	struct dwc_otg_flags sc_flags;
};

/* prototypes */

void dwc_otg_interrupt(struct dwc_otg_softc *);
int dwc_otg_init(struct dwc_otg_softc *);
void dwc_otg_uninit(struct dwc_otg_softc *);

#endif		/* _DWC_OTG_H_ */
