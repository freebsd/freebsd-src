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
 *	From: wd82371reg.h,v 1.3 1997/02/22 09:44:15 peter Exp $
 * $FreeBSD: src/sys/pci/ide_pcireg.h,v 1.2 1999/08/28 00:50:47 peter Exp $
 */

#ifndef _PCI_IDEPCIREG_H_
#define _PCI_IDEPCIREG_H_	1

/* Ports are for controller 0.  Add SFF8038_CTLR_1 for controller 1. */
#define	SFF8038_CTLR_1		8

/* Contents of BMICOM register */
#define	BMICOM_PORT		0
#define	BMICOM_READ_WRITE	0x0008 /* false = read, true = write */
#define	BMICOM_STOP_START	0x0001 /* false = stop, true = start */

/* Contents of BMISTA register */
#define	BMISTA_PORT		2
#define	BMISTA_SIMPLEX		0x0080 /* 1 = controller cannot DMA on both
					  channels simultaneously */
#define	BMISTA_DMA1CAP		0x0040 /* true = drive 1 can DMA */
#define	BMISTA_DMA0CAP		0x0020 /* true = drive 0 can DMA */
#define	BMISTA_INTERRUPT	0x0004
#define	BMISTA_DMA_ERROR	0x0002
#define	BMISTA_DMA_ACTIVE	0x0001

#define	BMIDTP_PORT		4      /* use outl */

struct ide_pci_prd {
	u_int32_t prd_base;
	u_int32_t prd_count;
};

#define	PRD_EOT_BIT		0x80000000

#endif /* _PCI_IDEPCIREG_H_ */
