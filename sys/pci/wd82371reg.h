/*
 * Copyright 1996 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  M.I.T. makes
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 * 
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id$
 */

#ifndef _PCI_WD82371REG_H_
#define _PCI_WD82371REG_H_	1

/* Contents of IDETM register, as two 16-bit words (high ctlr 1, low ctlr 0) */
#define	IDETM_ENABLE		0x8000
#define	IDETM_IORDY_SAMP	0x3000 /* 00:5, 01:4, 10:3, 11:2 clocks */
#define	IDETM_RECOVERY_TIME	0x0300 /* 00:4, 01:3, 10:2, 11:1 clocks */
#define	IDETM_TIMING_ENB_1	0x0080
#define	IDETM_PREFETCH_POST_1	0x0040
#define	IDETM_ISP_ENB_1		0x0020 /* enabled IORDY sampling */
#define	IDETM_FAST_TIMING_1	0x0010
#define	IDETM_TIMING_ENB_0	0x0008
#define	IDETM_PREFETCH_POST_0	0x0004
#define	IDETM_ISP_ENB_0		0x0002
#define	IDETM_FAST_TIMING_0	0x0001

#define	IDETM_CTLR_0(x)		(x)
#define	IDETM_CTLR_1(x)		((x) >> 16)

/* Ports are for controller 0.  Add PIIX_CTLR_1 for controller 1. */
#define	PIIX_CTLR_1		8

/* Contents of BMICOM register */
#define	BMICOM_PORT		0
#define	BMICOM_READ_WRITE	0x0008 /* false = read, true = write */
#define	BMICOM_STOP_START	0x0001 /* false = stop, true = start */

/* Contents of BMISTA register */
#define	BMISTA_PORT		2
#define	BMISTA_DMA1CAP		0x0040 /* true = drive 1 can DMA */
#define	BMISTA_DMA0CAP		0x0020 /* true = drive 0 can DMA */
#define	BMISTA_INTERRUPT	0x0004
#define	BMISTA_DMA_ERROR	0x0002
#define	BMISTA_DMA_ACTIVE	0x0001

#define	BMIDTP_PORT		4      /* use outl */

struct piix_prd {
	u_int32_t prd_base;
	u_int16_t prd_eot;
	u_int16_t prd_count;
};

#define	PRD_EOT_BIT		0x8000

#define	PRD_ALLOC_SIZE		4096
#define	PRD_MAX_SEGS		(PRD_ALLOC_SIZE / 4)

#endif /* _PCI_WD82371REG_H_ */
