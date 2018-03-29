/*	$FreeBSD$	*/
/*	$NecBSD: nspvar.h,v 1.7.14.5 2001/06/29 06:27:54 honda Exp $	*/
/*	$NetBSD$	*/

/*-
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1998, 1999, 2000, 2001
 *	NetBSD/pc98 porting staff. All rights reserved.
 *
 *  Copyright (c) 1998, 1999, 2000, 2001
 *	Naofumi HONDA. All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_NSPVAR_H_
#define	_NSPVAR_H_

/*****************************************************************
 * Host adapter structure
 *****************************************************************/
struct nsp_softc {
	struct scsi_low_softc sc_sclow;		/* generic data */

	int port_rid;
	int irq_rid;
	int mem_rid;
	struct resource *port_res;
	struct resource *irq_res;
	struct resource *mem_res;

	void *nsp_intrhand;

	int sc_tmaxcnt;				/* timeout count */
	int sc_seltout;				/* selection timeout counter */
	int sc_timer;				/* timer start */

	int sc_suspendio;			/* SMIT: data suspendio bytes */
	u_int8_t sc_xfermr;			/* SMIT: fifo control reg */
	int sc_dataout_timeout;			/* data out timeout counter */

	u_int sc_idbit;				/* host id bit pattern */
	u_int sc_cnt;				/* fifo R/W count (host) */

	u_int8_t sc_iclkdiv;			/* scsi chip clock divisor */
	u_int8_t sc_clkdiv;			/* asic chip clock divisor */
	u_int8_t sc_icr;			/* interrupt control reg */

	u_int8_t sc_busc;			/* busc registers */
	u_int8_t sc_parr;			/* parity control register */
};

/*****************************************************************
 * Lun information 
 *****************************************************************/
struct nsp_targ_info {
	struct targ_info nti_ti;		/* generic lun info */

	u_int8_t nti_reg_syncr;			/* sync registers per devices */
	u_int8_t nti_reg_ackwidth;		/* ackwidth per devices */
};

/*****************************************************************
 * Proto
 *****************************************************************/
int nspprobesubr(struct resource *, u_int);
void nspattachsubr(struct nsp_softc *);
int nspintr(void *);

#endif	/* !_NSPVAR_H_ */
